from __future__ import annotations

from pathlib import Path

import numpy as np

from .contracts import Bundle, ContractError, ModelContract


ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ0123456789-"
BLANK_INDEX = 34


def _model(bundle: Bundle, role: str) -> ModelContract:
    match = next((model for model in bundle.models if model.role == role), None)
    if match is None:
        raise ContractError(f"unknown model role: {role}")
    return match


def _image_input(model: ModelContract, image_path: Path) -> np.ndarray:
    try:
        from PIL import Image
    except ImportError as exc:
        raise ContractError("Pillow is unavailable; install the model-builder 'reference' extra") from exc
    if not image_path.is_file():
        raise ContractError(f"reference image is missing: {image_path}")
    with Image.open(image_path) as opened:
        image = opened.convert("RGB")
        if model.role == "lprnet":
            image = image.resize((156, 32), Image.Resampling.BILINEAR)
            array = np.asarray(image, dtype=np.float32)[..., ::-1].copy()
            return array[None, ...]
        target = model.input_shape[-1]
        width, height = image.size
        scale = min(target / width, target / height)
        resized = image.resize(
            (max(1, round(width * scale)), max(1, round(height * scale))),
            Image.Resampling.BILINEAR,
        )
        canvas = np.full((target, target, 3), model.pad_value, dtype=np.float32)
        array = np.asarray(resized, dtype=np.float32)
        canvas[: array.shape[0], : array.shape[1], :] = array
        normalized = (canvas - np.asarray(model.mean, dtype=np.float32)) / np.asarray(
            model.std, dtype=np.float32
        )
        return np.transpose(normalized, (2, 0, 1))[None, ...]


def _decode_ctc_sample(logits: np.ndarray) -> tuple[str, float]:
    selected = np.argmax(logits, axis=1)
    probabilities = np.exp(logits - np.max(logits, axis=1, keepdims=True))
    probabilities /= np.sum(probabilities, axis=1, keepdims=True)
    text: list[str] = []
    confidences: list[float] = []
    previous = -1
    for step, index in enumerate(selected.tolist()):
        if index != BLANK_INDEX and index != previous:
            text.append(ALPHABET[index])
            confidences.append(float(probabilities[step, index]))
        previous = index
    return "".join(text), float(np.mean(confidences)) if confidences else 0.0


def decode_ctc_batch(logits: np.ndarray) -> list[tuple[str, float]]:
    if logits.ndim != 3 or logits.shape[0] < 1 or logits.shape[1:] != (39, 35):
        raise ContractError(f"LPR output must be [N,39,35], got {list(logits.shape)}")
    return [_decode_ctc_sample(logits[index]) for index in range(logits.shape[0])]


def decode_ctc(logits: np.ndarray) -> tuple[str, float]:
    if logits.shape != (1, 39, 35):
        raise ContractError(f"LPR output must be [1,39,35], got {list(logits.shape)}")
    return decode_ctc_batch(logits)[0]


def run_reference(bundle: Bundle, role: str, image_path: Path) -> dict[str, object]:
    try:
        import onnxruntime as ort
    except ImportError as exc:
        raise ContractError(
            "ONNX Runtime is unavailable; install the model-builder 'reference' extra"
        ) from exc
    model = _model(bundle, role)
    input_tensor = _image_input(model, image_path)
    session = ort.InferenceSession(str(model.source), providers=["CPUExecutionProvider"])
    outputs = session.run(None, {model.input_name: input_tensor})
    report: dict[str, object] = {
        "schema": "mbfs.onnx-reference/v1",
        "bundle": bundle.version,
        "role": role,
        "image": image_path.name,
        "input_shape": list(input_tensor.shape),
        "outputs": [
            {
                "name": metadata.name,
                "shape": list(value.shape),
                "minimum": float(np.min(value)),
                "maximum": float(np.max(value)),
                "finite": bool(np.all(np.isfinite(value))),
            }
            for metadata, value in zip(session.get_outputs(), outputs, strict=True)
        ],
    }
    if role == "lprnet":
        text, confidence = decode_ctc(outputs[0])
        report["text"] = text
        report["confidence"] = confidence
    return report
