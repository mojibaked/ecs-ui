#!/usr/bin/env python3
"""
Stage 8 texelotl native-vs-clay screenshot gate.

Historical artifact: this helper depended on the temporary comparison build
that existed before the native-only cutover. It is kept for audit history and is
not a runnable current verification tool.

This is a verification helper, not product code. It launches texelotl desktop
under Xvfb, drives the attach socket through representative UI states, captures
native-layout and clay-layout screenshots at scale 1 and 2, then exact-compares
the pixels.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - verification host dependency
    raise SystemExit("Pillow is required for pixel comparison") from exc


NODE_RE = re.compile(
    r'id="([^"]*)".*?layout=\(([-0-9.]+),([-0-9.]+) '
    r'([-0-9.]+)x([-0-9.]+)\)')


class TexelotlRun:
    def __init__(
        self,
        *,
        texelotl_root: Path,
        out_dir: Path,
        display: str,
        backend: str,
        label: str,
        scale: int,
        env_extra: dict[str, str] | None = None,
        window_size: tuple[int, int] | None = None,
    ) -> None:
        self.texelotl_root = texelotl_root
        self.build_dir = texelotl_root / "build-desktop"
        self.desktop = self.build_dir / "texelotl_desktop"
        self.eval_bin = self.build_dir / "texelotl-eval"
        self.backend = backend
        self.label = label
        self.scale = scale
        self.dir = out_dir / f"{label}-scale{scale}"
        self.dir.mkdir(parents=True, exist_ok=True)
        self.state_dir = self.dir / "states"
        self.state_dir.mkdir(parents=True, exist_ok=True)
        self.socket = str(self.dir / "texelotl.sock")
        self.paths: dict[str, Path] = {}

        env = os.environ.copy()
        env.pop("ECS_UI_FRAME_BACKEND", None)
        env.update({
            "DISPLAY": display,
            "TEXELOTL_SOCKET": self.socket,
            "TEXELOTL_DESKTOP_NO_WAIT_EVENTS": "1",
            "TEXELOTL_DESKTOP_UI_SCALE": str(scale),
        })
        if env_extra is not None:
            env.update(env_extra)
        if backend == "clay":
            env["ECS_UI_FRAME_BACKEND"] = "clay"

        self.log = (self.dir / "app.log").open("wb")
        self.proc = subprocess.Popen(
            [str(self.desktop)],
            cwd=str(self.build_dir),
            env=env,
            stdout=self.log,
            stderr=subprocess.STDOUT,
        )
        for _ in range(80):
            try:
                if self.eval({"cmd": "ping"}, quiet=True).get("ok"):
                    break
            except Exception:
                time.sleep(0.1)
        else:
            raise RuntimeError(f"{backend} scale {scale}: attach ping failed")
        self.eval({"cmd": "ticks", "count": 4})
        if window_size is not None:
            self.resize_window(display, window_size[0], window_size[1])

    def close(self) -> None:
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=2)
        self.log.close()

    def eval(self, payload: dict[str, object], *, quiet: bool = False) -> dict:
        env = os.environ.copy()
        env["TEXELOTL_SOCKET"] = self.socket
        proc = subprocess.run(
            [str(self.eval_bin), json.dumps(payload, separators=(",", ":"))],
            cwd=str(self.build_dir),
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=15,
            check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"eval failed {payload}: rc={proc.returncode} "
                f"stdout={proc.stdout!r} stderr={proc.stderr!r}")
        data = json.loads(proc.stdout)
        if not data.get("ok", False):
            raise RuntimeError(f"eval not ok {payload}: {data}")
        if quiet:
            return data
        return data

    def nodes(self) -> dict[str, list[tuple[float, float, float, float]]]:
        tree = self.eval({"cmd": "tree"})["tree"]
        nodes: dict[str, list[tuple[float, float, float, float]]] = {}
        for line in tree.splitlines():
            match = NODE_RE.search(line)
            if match is None:
                continue
            rect = tuple(float(match.group(i)) for i in range(2, 6))
            nodes.setdefault(match.group(1), []).append(rect)
        return nodes

    def has_node(self, stable_id: str) -> bool:
        return stable_id in self.nodes()

    def require_node(self, stable_id: str, index: int = 0) -> None:
        nodes = self.nodes()
        if stable_id not in nodes or index >= len(nodes[stable_id]):
            raise RuntimeError(
                f"{self.backend} scale {self.scale}: expected "
                f"{stable_id}[{index}] to be present")

    def rect(
        self,
        stable_id: str,
        index: int = 0,
    ) -> tuple[float, float, float, float]:
        nodes = self.nodes()
        if stable_id not in nodes or index >= len(nodes[stable_id]):
            raise RuntimeError(
                f"{self.backend} scale {self.scale}: missing "
                f"{stable_id}[{index}]")
        return nodes[stable_id][index]

    def point_in(
        self,
        stable_id: str,
        *,
        fx: float = 0.5,
        fy: float = 0.5,
        index: int = 0,
    ) -> tuple[float, float]:
        x, y, width, height = self.rect(stable_id, index)
        return (x + width * fx) * self.scale, (y + height * fy) * self.scale

    def pointer(
        self,
        action: str,
        x: float,
        y: float,
        button: str | None = None,
    ) -> None:
        payload: dict[str, object] = {
            "cmd": "pointer",
            "action": action,
            "x": round(x, 2),
            "y": round(y, 2),
        }
        if button is not None:
            payload["button"] = button
        self.eval(payload)

    def click(
        self,
        stable_id: str,
        *,
        index: int = 0,
        button: str = "primary",
        fx: float = 0.5,
        fy: float = 0.5,
    ) -> None:
        x, y = self.point_in(stable_id, fx=fx, fy=fy, index=index)
        self.pointer("move", x, y)
        self.pointer("down", x, y, button)
        self.pointer("up", x, y, button)

    def open_file_menu(self) -> None:
        for _ in range(3):
            self.click("ToolbarFileButton")
            self.eval({"cmd": "ticks", "count": 2})
            if self.has_node("FileMenuRow"):
                return
        raise RuntimeError(
            f"{self.backend} scale {self.scale}: file menu did not open")

    def drag(
        self,
        stable_id: str,
        fx1: float,
        fy1: float,
        fx2: float,
        fy2: float,
        *,
        button: str = "primary",
    ) -> None:
        x1, y1 = self.point_in(stable_id, fx=fx1, fy=fy1)
        x2, y2 = self.point_in(stable_id, fx=fx2, fy=fy2)
        self.pointer("move", x1, y1)
        self.pointer("down", x1, y1, button)
        self.pointer("move", x2, y2)
        self.pointer("up", x2, y2, button)

    def scroll(self, stable_id: str, *, dy: float, dx: float = 0.0) -> None:
        x, y = self.point_in(stable_id)
        self.eval({
            "cmd": "scroll",
            "x": round(x, 2),
            "y": round(y, 2),
            "dx": dx,
            "dy": dy,
        })

    def key(self, name: str) -> None:
        self.eval({"cmd": "key", "name": name})

    def text(self, value: str) -> None:
        self.eval({"cmd": "text", "string": value})

    def script(self, source: str, *, name: str = "stage8") -> dict:
        return self.eval({
            "cmd": "script",
            "lang": "lua",
            "name": name,
            "source": source,
        })

    def state(self) -> dict:
        return self.eval({"cmd": "state"})

    def wait_until(
        self,
        label: str,
        predicate,
        *,
        timeout_seconds: float = 6.0,
    ) -> dict:
        deadline = time.monotonic() + timeout_seconds
        last = {}
        while time.monotonic() < deadline:
            last = self.state()
            if predicate(last):
                return last
            self.eval({"cmd": "ticks", "count": 2})
            time.sleep(0.05)
        raise RuntimeError(
            f"{self.backend} scale {self.scale}: timed out waiting for {label}; "
            f"last={json.dumps(last, sort_keys=True)[:1000]}")

    def resize_window(self, display: str, width: int, height: int) -> None:
        xdotool = shutil.which("xdotool")
        if xdotool is None:
            raise RuntimeError("xdotool is required for the small-window gate")
        env = os.environ.copy()
        env["DISPLAY"] = display
        search = subprocess.run(
            [xdotool, "search", "--pid", str(self.proc.pid)],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
            check=False,
        )
        ids = [line.strip() for line in search.stdout.splitlines() if line.strip()]
        if search.returncode != 0 or not ids:
            raise RuntimeError(
                f"xdotool could not find texelotl window: "
                f"rc={search.returncode} stderr={search.stderr!r}")
        window_id = ids[-1]
        subprocess.run(
            [xdotool, "windowmove", window_id, "37", "29"],
            env=env,
            timeout=5,
            check=True,
        )
        subprocess.run(
            [xdotool, "windowsize", "--sync", window_id, str(width), str(height)],
            env=env,
            timeout=5,
            check=True,
        )
        for _ in range(30):
            self.eval({"cmd": "ticks", "count": 2})
            state = self.state()
            window = state.get("window", {})
            if int(window.get("width", 0)) == width and int(window.get("height", 0)) == height:
                return
            time.sleep(0.05)
        raise RuntimeError(
            f"{self.backend} scale {self.scale}: window did not resize to "
            f"{width}x{height}; last={self.state().get('window')}")

    def screenshot(self, name: str) -> Path:
        full_name = f"stage8-{self.label}-s{self.scale}-{name}"
        reply = self.eval({"cmd": "screenshot", "name": full_name})
        source = Path(reply["path"])
        target = self.dir / f"{name}.png"
        shutil.copyfile(source, target)
        self.paths[name] = target
        state = self.eval({"cmd": "state"})
        (self.state_dir / f"{name}.json").write_text(
            json.dumps(state, indent=2) + "\n")
        return target


def compare_images(
    native: Path,
    clay: Path,
    masks: list[tuple[int, int, int, int]] | None = None,
) -> dict[str, object]:
    native_image = Image.open(native).convert("RGBA")
    clay_image = Image.open(clay).convert("RGBA")
    if native_image.size != clay_image.size:
        return {
            "same": False,
            "count": None,
            "bbox": None,
            "first": ("size", native_image.size, clay_image.size),
        }

    native_pixels = native_image.load()
    clay_pixels = clay_image.load()
    width, height = native_image.size
    count = 0
    first = None
    min_x = width
    min_y = height
    max_x = -1
    max_y = -1
    masks = masks or []
    for y in range(height):
        for x in range(width):
            if any(
                x >= left and x <= right and y >= top and y <= bottom
                for left, top, right, bottom in masks):
                continue
            if native_pixels[x, y] == clay_pixels[x, y]:
                continue
            count += 1
            if first is None:
                first = (x, y, native_pixels[x, y], clay_pixels[x, y])
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
    return {
        "same": count == 0,
        "count": count,
        "bbox": None if count == 0 else (min_x, min_y, max_x, max_y),
        "first": first,
        "size": native_image.size,
        "masked_regions": masks,
    }


def prompt_selection_mask(image_path: Path) -> tuple[int, int, int, int] | None:
    state_path = image_path.parent / "states" / f"{image_path.stem}.json"
    if not state_path.exists():
        return None
    state = json.loads(state_path.read_text())
    prompt = state.get("prompt_bar", {})
    selection = state.get("selection", {})
    camera = state.get("camera", {})
    surface = prompt.get("canvas_surface", {})
    if not (
        selection.get("has_selection") and camera.get("has_document") and
        surface):
        return None
    zoom = float(camera.get("zoom", 0.0))
    scale_match = re.search(r"scale([0-9]+)", image_path.parent.name)
    ui_scale = float(scale_match.group(1)) if scale_match is not None else 1.0
    doc_width = float(camera.get("document_width", 0.0))
    doc_height = float(camera.get("document_height", 0.0))
    if zoom <= 0.0 or doc_width <= 0.0 or doc_height <= 0.0:
        return None
    surface_x = float(surface.get("x", 0.0))
    surface_y = float(surface.get("y", 0.0))
    surface_w = float(surface.get("width", 0.0))
    surface_h = float(surface.get("height", 0.0))
    physical_zoom = zoom * ui_scale
    document_x = surface_x + (surface_w - doc_width * physical_zoom) * 0.5
    document_y = surface_y + (surface_h - doc_height * physical_zoom) * 0.5
    left = document_x + float(selection.get("x", 0.0)) * physical_zoom
    top = document_y + float(selection.get("y", 0.0)) * physical_zoom
    right = left + float(selection.get("width", 0.0)) * physical_zoom
    bottom = top + float(selection.get("height", 0.0)) * physical_zoom
    return (
        math.floor(left),
        math.floor(top),
        math.ceil(right),
        math.ceil(bottom),
    )


def masks_for_comparison(
    name: str,
    left: Path,
    right: Path,
) -> list[tuple[int, int, int, int]]:
    if not name.startswith("prompt-"):
        return []
    masks = []
    for path in (left, right):
        mask = prompt_selection_mask(path)
        if mask is not None and mask not in masks:
            masks.append(mask)
    return masks


def prepare_prompt_bar(run: TexelotlRun) -> None:
    run.script(
        """
local doc = tx.read.active_document_id()
assert(doc, "active document missing")
tx.submit.selection_select_rect(doc, 2, 2, 10, 10)
tx.await()
tx.await()
local state = tx.read.state()
assert(state.prompt_bar and state.prompt_bar.visible, "prompt bar hidden")
return true
""",
        name="stage8-prompt-selection",
    )
    run.wait_until(
        "prompt bar field",
        lambda state: state.get("prompt_bar", {}).get("visible", False),
    )
    run.require_node("PromptBarField")


def submit_prompt(run: TexelotlRun, value: str) -> None:
    run.click("PromptBarField")
    run.text(value)
    run.eval({"cmd": "ticks", "count": 2})
    run.require_node("PromptBarSubmitButton")
    run.click("PromptBarSubmitButton")
    run.eval({"cmd": "ticks", "count": 2})


def drive_prompt_success(run: TexelotlRun) -> None:
    prepare_prompt_bar(run)
    submit_prompt(run, "stage8 prompt preview")
    run.wait_until(
        "prompt busy",
        lambda state: state.get("prompt_bar", {}).get("busy", False),
    )
    run.screenshot("prompt-busy")
    run.wait_until(
        "prompt preview",
        lambda state: state.get("prompt_bar", {}).get("has_preview", False),
        timeout_seconds=10.0,
    )
    run.require_node("PromptBarAcceptButton")
    run.require_node("PromptBarRejectButton")
    run.screenshot("prompt-preview")


def drive_prompt_error(run: TexelotlRun) -> None:
    prepare_prompt_bar(run)
    submit_prompt(run, "stage8 prompt error")
    run.wait_until(
        "prompt error",
        lambda state: state.get("prompt_bar", {}).get("has_error", False),
        timeout_seconds=10.0,
    )
    run.require_node("PromptBarRetryButton")
    run.require_node("PromptBarDismissButton")
    run.screenshot("prompt-error")


def drive_small_window(run: TexelotlRun) -> None:
    run.screenshot("small-window-idle")
    run.open_file_menu()
    run.click("FileMenuRow", index=0)
    run.eval({"cmd": "ticks", "count": 3})
    run.require_node("FileDialogPanel")
    run.require_node("FileDialogList")
    run.screenshot("small-window-open-dialog")


def drive(run: TexelotlRun) -> None:
    run.screenshot("idle")

    zoom_before = run.eval({"cmd": "state"})["camera"]
    run.scroll("CanvasSurface", dy=3.0)
    run.eval({"cmd": "ticks", "count": 2})
    zoom_after = run.eval({"cmd": "state"})["camera"]
    (run.dir / "canvas_zoom_state.json").write_text(
        json.dumps({"before": zoom_before, "after": zoom_after}, indent=2)
        + "\n")
    run.screenshot("canvas-zoom")

    tool_x, tool_y = run.point_in("ToolbarTool")
    run.pointer("move", tool_x, tool_y)
    run.eval({"cmd": "ticks", "count": 2})
    run.screenshot("toolbar-hover")
    run.pointer("down", tool_x, tool_y, "primary")
    run.eval({"cmd": "ticks", "count": 1})
    run.screenshot("toolbar-pressed")
    run.pointer("up", tool_x, tool_y, "primary")
    run.eval({"cmd": "ticks", "count": 2})

    run.open_file_menu()
    run.screenshot("file-menu")

    run.click("FileMenuRow", index=0)
    run.eval({"cmd": "ticks", "count": 3})
    run.require_node("FileDialogPanel")
    run.require_node("FileDialogList")
    run.require_node("FileDialogPathField")
    run.screenshot("open-dialog-top")

    run.click("FileDialogPathField")
    run.text("stage8-long-text-value-for-layout")
    run.key("left")
    run.key("left")
    run.screenshot("open-dialog-caret")

    run.drag("FileDialogPathField", 0.15, 0.5, 0.55, 0.5)
    run.screenshot("open-dialog-field-drag")

    run.scroll("FileDialogList", dy=-8.0)
    run.eval({"cmd": "ticks", "count": 2})
    run.screenshot("open-dialog-scroll-mid")

    run.scroll("FileDialogList", dy=-30.0)
    run.eval({"cmd": "ticks", "count": 2})
    run.screenshot("open-dialog-scroll-bottom")

    blocked_before = run.eval({"cmd": "state"})["camera"]
    canvas_x, canvas_y = run.point_in("CanvasSurface")
    run.pointer("move", canvas_x, canvas_y)
    run.pointer("down", canvas_x, canvas_y, "middle")
    run.pointer(
        "move",
        canvas_x + 80.0 * run.scale,
        canvas_y + 40.0 * run.scale,
    )
    run.pointer(
        "up",
        canvas_x + 80.0 * run.scale,
        canvas_y + 40.0 * run.scale,
        "middle",
    )
    blocked_after = run.eval({"cmd": "state"})["camera"]
    (run.dir / "scrim_middle_drag_camera.json").write_text(
        json.dumps({"before": blocked_before, "after": blocked_after}, indent=2)
        + "\n")
    run.screenshot("dialog-after-blocked-middle-drag")

    run.key("escape")
    run.eval({"cmd": "ticks", "count": 3})
    run.open_file_menu()
    run.click("FileMenuRow", index=2)
    run.eval({"cmd": "ticks", "count": 3})
    run.require_node("FileDialogPanel")
    run.screenshot("save-as-dialog")

    run.key("escape")
    run.eval({"cmd": "ticks", "count": 3})
    run.open_file_menu()
    run.click("FileMenuRow", index=3)
    run.eval({"cmd": "ticks", "count": 3})
    run.require_node("FileDialogPanel")
    run.screenshot("export-dialog")

    run.key("escape")
    run.eval({"cmd": "ticks", "count": 3})
    canvas_x, canvas_y = run.point_in("CanvasSurface")
    run.pointer("move", canvas_x, canvas_y)
    run.pointer("down", canvas_x, canvas_y, "secondary")
    run.pointer("up", canvas_x, canvas_y, "secondary")
    run.eval({"cmd": "ticks", "count": 2})
    run.require_node("ContextMenuPanel")
    run.screenshot("canvas-context-menu")

    run.key("escape")
    run.eval({"cmd": "ticks", "count": 3})
    run.scroll("PaletteSwatchList", dy=-5.0)
    run.eval({"cmd": "ticks", "count": 2})
    run.screenshot("palette-scroll-attempt")

    drive_prompt_success(run)
    run.click("PromptBarRejectButton")
    run.eval({"cmd": "ticks", "count": 4})

    run.eval({"cmd": "ticks", "count": 5})
    run.screenshot("idle-after-tour")


def require_no_parent_backend_env() -> None:
    if "ECS_UI_FRAME_BACKEND" in os.environ:
        raise RuntimeError(
            "unset ECS_UI_FRAME_BACKEND before running the gate; the driver "
            "sets backend selection explicitly per child process")


def require_clay_liveness(texelotl_root: Path, symbol: str) -> None:
    desktop = texelotl_root / "build-desktop" / "texelotl_desktop"
    proc = subprocess.run(
        ["nm", str(desktop)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=15,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"nm failed for clay liveness check: rc={proc.returncode} "
            f"stderr={proc.stderr!r}")
    if symbol not in proc.stdout:
        raise RuntimeError(
            f"clay liveness check failed: {symbol!r} not found in {desktop}; "
            "configure build-desktop with a Clay checkout before trusting "
            "native-vs-clay results")


def start_xvfb(display: str, out_dir: Path) -> subprocess.Popen:
    xvfb_log = (out_dir / "xvfb.log").open("wb")
    xvfb = subprocess.Popen(
        ["Xvfb", display, "-screen", "0", "1280x800x24"],
        stdout=xvfb_log,
        stderr=subprocess.STDOUT,
    )
    time.sleep(1)
    if xvfb.poll() is not None:
        xvfb_log.close()
        raise RuntimeError(
            f"Xvfb failed to start on {display}; see {out_dir / 'xvfb.log'}")
    return xvfb


def drive_for_tour(run: TexelotlRun, tour: str) -> None:
    if tour == "prompt":
        drive_prompt_success(run)
    elif tour == "small":
        drive_small_window(run)
    else:
        drive(run)


def run_one(
    *,
    texelotl_root: Path,
    out_dir: Path,
    display: str,
    side: str,
    backend: str,
    scale: int,
    tour: str,
    env_extra: dict[str, str],
    window_size: tuple[int, int] | None = None,
) -> dict[str, Path]:
    label = backend if side == backend else f"{side}-{backend}"
    run = TexelotlRun(
        texelotl_root=texelotl_root,
        out_dir=out_dir,
        display=display,
        backend=backend,
        label=label,
        scale=scale,
        env_extra=env_extra,
        window_size=window_size,
    )
    try:
        drive_for_tour(run, tour)
        return dict(run.paths)
    finally:
        run.close()


def run_gate(args: argparse.Namespace) -> int:
    require_no_parent_backend_env()
    texelotl_root = Path(args.texelotl_root).resolve()
    out_dir = Path(args.out).resolve()
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if args.right_backend == "clay" or args.left_backend == "clay":
        require_clay_liveness(texelotl_root, args.clay_symbol)

    xvfb = None
    display = args.display
    if args.start_xvfb:
        xvfb = start_xvfb(display, out_dir)
    if args.self_check_only:
        if xvfb is not None:
            xvfb.terminate()
            xvfb.wait(timeout=2)
        print("SELF_CHECK_OK")
        return 0

    runs: dict[tuple[str, int], dict[str, Path]] = {}
    try:
        success_env = {
            "TEXELOTL_GENAI_MOCK": "1",
            "TEXELOTL_GENAI_MOCK_DELAY_MS": "900",
        }
        error_env = {
            "TEXELOTL_GENAI_MOCK": "1",
            "TEXELOTL_GENAI_MOCK_FAIL": "1",
            "TEXELOTL_GENAI_MOCK_DELAY_MS": "300",
        }
        for scale in (1, 2):
            sides = (("left", args.left_backend), ("right", args.right_backend))
            for side, backend in sides:
                paths = run_one(
                    texelotl_root=texelotl_root,
                    out_dir=out_dir,
                    display=display,
                    side=side,
                    backend=backend,
                    scale=scale,
                    tour=args.tour,
                    env_extra=success_env,
                )
                if args.tour == "full":
                    error_label = backend if f"{side}-error" == backend else f"{side}-error-{backend}"
                    error_run = TexelotlRun(
                        texelotl_root=texelotl_root,
                        out_dir=out_dir,
                        display=display,
                        backend=backend,
                        label=error_label,
                        scale=scale,
                        env_extra=error_env,
                    )
                    try:
                        drive_prompt_error(error_run)
                        error_paths = dict(error_run.paths)
                    finally:
                        error_run.close()
                    paths.update(error_paths)
                    small_paths = run_one(
                        texelotl_root=texelotl_root,
                        out_dir=out_dir,
                        display=display,
                        side=f"{side}-small",
                        backend=backend,
                        scale=scale,
                        tour="small",
                        env_extra=success_env,
                        window_size=(640, 480),
                    )
                    paths.update(small_paths)
                runs[(side, scale)] = paths

        names = sorted(runs[("left", 1)].keys())
        report = []
        any_delta = False
        for scale in (1, 2):
            for name in names:
                left = runs[("left", scale)][name]
                right = runs[("right", scale)][name]
                diff = compare_images(
                    left,
                    right,
                    masks=masks_for_comparison(name, left, right),
                )
                diff.update({
                    "scale": scale,
                    "name": name,
                    "left_backend": args.left_backend,
                    "right_backend": args.right_backend,
                    "left": str(left),
                    "right": str(right),
                })
                report.append(diff)
                any_delta = any_delta or not diff["same"]
        (out_dir / "comparison-report.json").write_text(
            json.dumps(report, indent=2, default=str) + "\n")
        for item in report:
            if item["same"]:
                print(f"scale {item['scale']} {item['name']}: diff=0")
            else:
                print(
                    f"scale {item['scale']} {item['name']}: "
                    f"DELTA count={item['count']} bbox={item['bbox']} "
                    f"first={item['first']}")
        print("RESULT", "DELTA" if any_delta else "ZERO_DELTAS")
        return 1 if any_delta else 0
    finally:
        if xvfb is not None:
            xvfb.terminate()
            try:
                xvfb.wait(timeout=2)
            except subprocess.TimeoutExpired:
                xvfb.kill()
                xvfb.wait(timeout=2)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--texelotl-root",
        default="/home/mojibake/repos/texelotl",
        help="texelotl checkout containing build-desktop",
    )
    parser.add_argument(
        "--out",
        default="/tmp/texelotl-stage8-ab",
        help="directory for screenshots, state JSON, and comparison report",
    )
    parser.add_argument("--display", default=":98")
    parser.add_argument(
        "--left-backend",
        choices=("native", "clay"),
        default="native",
        help="backend for the left/reference capture",
    )
    parser.add_argument(
        "--right-backend",
        choices=("native", "clay"),
        default="clay",
        help="backend for the right/comparison capture",
    )
    parser.add_argument(
        "--tour",
        choices=("full", "prompt", "small"),
        default="full",
        help="drive the full tour or a focused prompt/small-window tour",
    )
    parser.add_argument(
        "--clay-symbol",
        default="Clay_BeginLayout",
        help="symbol that must exist in texelotl_desktop before clay captures",
    )
    parser.add_argument(
        "--self-check-only",
        action="store_true",
        help="run environment, clay-liveness, and optional Xvfb checks only",
    )
    parser.add_argument(
        "--start-xvfb",
        action="store_true",
        help="launch and clean up Xvfb for the run",
    )
    return run_gate(parser.parse_args())


if __name__ == "__main__":
    sys.exit(main())
