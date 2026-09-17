from __future__ import annotations

import numpy as np

from ds_model.reference import ALPHABET, BLANK_INDEX, decode_ctc, decode_ctc_batch


def test_ctc_decodes_repeated_characters_deterministically() -> None:
    logits = np.full((1, 39, 35), -10.0, dtype=np.float32)
    logits[:, :, BLANK_INDEX] = 10.0
    expected = "89AA15689"
    step = 0
    previous = ""
    for character in expected:
        if character == previous:
            step += 1
        logits[0, step, BLANK_INDEX] = -10.0
        logits[0, step, ALPHABET.index(character)] = 10.0
        previous = character
        step += 1
    text, confidence = decode_ctc(logits)
    assert text == expected
    assert confidence > 0.99
    batch = np.concatenate([logits, logits], axis=0)
    decoded = decode_ctc_batch(batch)
    assert [item[0] for item in decoded] == [expected, expected]
