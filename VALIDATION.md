# Validation — 6 September 2026

Tested on this Windows PC: NVIDIA GTX 1650 with 4 GB VRAM and approximately 15 GB system RAM.

- Windows executable packaged with Electron 40.10.6.
- Seven automated adapter and validation tests passed.
- Actual Electron UI checks passed: raw transcription mode, copy, clear, mode selection, and rejection of remote endpoints.
- Synthetic microphone test passed: 16 kHz mono PCM WAV capture, stop, discard, microphone release, transcription-to-editing flow. This does not substitute for checking the user's physical microphone.
- Packaged app started both native inference engines automatically and completed real speech recognition and prompt rewriting.
- Unauthenticated requests to both managed engines returned HTTP 401.
- NVIDIA's public `jfk.wav` sample (approximately 11 seconds) transcribed in 846 ms after model loading on CUDA0. This single sample is not a general latency or accuracy benchmark.
- Prompt version 1.2.0 passed 10/10 keyword regression checks, with manual review of outputs. Three cases mirror supplied prompt examples; seven are held out. The final correction example is distinct from the correction test. Short rewrites in the final run took approximately 1.1–3.7 seconds. See `evaluation.json` for exact inputs and outputs. Earlier results are in `evaluation-baseline.json`.
- Downloaded NeMo runtime, llama.cpp runtime, and both models passed SHA-256 verification against their upstream metadata.
- The restore/setup script parsed successfully; downloads and extraction were performed during development, but a second clean-machine execution of the entire restore script was not tested.

Known limits: batch transcription on stop, two-minute recordings, clipboard-based transfer, no automatic insertion into other applications, unsigned portable executable. A small language model can still alter meaning outside this test set; review before copying. The app is tested on this Windows machine, not certified across other Windows configurations.
