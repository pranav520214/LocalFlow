# LocalFlow

A Windows desktop dictation and prompt-editing app. Runs NVIDIA **NeMo-Speech.cpp 0.1.0** with **Nemotron 3.5 ASR Streaming 0.6B Q8**, and **llama.cpp b10819** with the official **Qwen3 1.7B Q8_0** model. The desktop shell uses Electron; both inference engines use native C++. This is an independent app inspired by the voice-to-text workflow, not affiliated with Wispr.

## Open the app

Double-click **Launch.cmd**. The bundled engines start automatically. Wait for **Both engines ready**. If needed, open **Local engines** and click **Start local engines** to retry. Back in **Voice studio**, select a mode and click Record, or use **Ctrl+Shift+Space** to start and stop from another app. Review your result, click **Copy text**, and paste it where you want.

- **Just transcribe:** speech to text without the language model.
- **Clean dictation:** remove filler and fix punctuation while retaining meaning.
- **Engineer a prompt:** turn a rough request into an instruction for another AI, preserving supplied constraints.

You can also type or paste a rough idea into **Your words** and click **Refine my words**. Both text areas are editable. Recordings stop at two minutes. Closing the window keeps the app running in the system tray; **Quit** in the tray closes it and its managed engines. Keep the complete LocalFlow folder together: the packaged executable uses the models and runtimes at its root.

## Local setup and operation

The prepared app includes its dependencies and models when setup has completed. To restore missing components on a Windows x64 machine, run `./setup.ps1` in PowerShell with Node.js installed. Setup downloads approximately 2.8 GB, verifies SHA-256 checksums, and installs app dependencies. `./setup.ps1 -Backend cpu` selects a CPU speech runtime for machines without CUDA support. Setup requires internet; normal operation with the downloaded local models does not.

This configuration uses the GPU for speech and CPU for editing to accommodate the verified GTX 1650 4 GB. RAM observed during development: approximately 15 GB. Actual latency depends on hardware and utterance length. The app checks engine readiness; it does not claim a model is running merely because a process was started.

App-managed servers require random session keys held in memory, with browser cross-origin access restricted. Only loopback HTTP endpoints at `127.0.0.1` are allowed; redirects are rejected. Settings default to speech port 8178 and editor port 8179. The renderer has no network access and no Node.js access. Audio is captured as mono PCM WAV and submitted through the main process to the local speech server. It is not written to an audio file. Transcript history is not persisted. Settings and Chromium runtime data are in `data/`; generated text is held in the current window. Windows pagefiles/crash tools and clipboard history are outside the app's retention controls. Models and runtimes make no cloud inference calls when launched by this app with local paths.

## Development

`npm start` launches from source. `npm test` runs adapter and input-validation tests. `node tests/ui-smoke.cjs` tests the actual Electron window with Playwright. `node tests/evaluate-packaged.cjs` exercises ten prompt regression cases through the packaged app and writes `evaluation.json`. Three cases mirror examples in the prompt; seven are held out. `tests/evaluate.cjs` is available for a separately started editor without authentication. `npm run package` creates `dist/LocalFlow-win32-x64/LocalFlow.exe`; keep it within this directory structure.

Prompt templates are versioned in `src/prompts.cjs`. Generation uses temperature 0.2, a 1,200-token output limit, 4,096-token model context, and disabled thinking. Keyword regression checks are a limited signal, not a measure of general prompt quality. Review generated text: a small model can still omit or change details. Inputs above 12,000 characters are rejected; token-heavy inputs can reach the model context limit earlier and return an error.

## Current scope

The app provides toggle recording, batch transcription after stopping, prompt editing, a tray, and copy/paste through the clipboard. It does not yet inject text automatically into other apps, display streaming partial transcripts, use per-app context, or provide an installer, auto-updater, or signed executable. Existing text is kept when editing fails. A failed transcription is reported and the in-memory recording discarded; it cannot be retried without recording again.

## Sources and licenses

- [NVIDIA NeMo-Speech.cpp](https://github.com/NVIDIA/NeMo-Speech.cpp), Apache-2.0; downloaded runtime includes its license notices.
- [NVIDIA Nemotron 3.5 speech model](https://huggingface.co/nvidia/nemotron-3.5-asr-streaming-0.6b), NVIDIA Open Model License (OpenMDW 1.1), as identified in NVIDIA's model registry.
- [Official Qwen3 1.7B GGUF](https://huggingface.co/Qwen/Qwen3-1.7B-GGUF), Apache-2.0.
- [llama.cpp](https://github.com/ggml-org/llama.cpp), MIT.
- Electron, MIT. Original LocalFlow app code is MIT; bundled third-party components retain their licenses.

