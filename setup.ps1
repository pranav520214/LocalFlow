param([ValidateSet('cuda','cpu')][string]$Backend='cuda')
$ErrorActionPreference='Stop'
$appRoot=$PSScriptRoot
$downloadDir=Join-Path $appRoot 'work\downloads'
New-Item -ItemType Directory -Force $downloadDir,(Join-Path $appRoot 'models'),(Join-Path $appRoot 'runtimes') | Out-Null
function Get-Verified([string]$Url,[string]$Target,[string]$Sha) {
    if((Test-Path -LiteralPath $Target) -and (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash -eq $Sha){return}
    $partial=$Target+'.part'
    Write-Host ('Downloading '+[IO.Path]::GetFileName($Target))
    & curl.exe --fail --location --proto '=https' --proto-redir '=https' --retry 3 --continue-at - --output $partial $Url
    if($LASTEXITCODE -ne 0){throw 'Download failed. Rerun setup to resume.'}
    if((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $Sha){throw ('Checksum mismatch: '+$partial)}
    Move-Item -LiteralPath $partial -Destination $Target -Force
}
$nemoName="nemo-speech-0.1.0-windows-x86_64-$Backend.zip"
$nemoUrl="https://github.com/NVIDIA/NeMo-Speech.cpp/releases/download/v0.1.0/$nemoName"
# Obtain the release checksum from NVIDIA over HTTPS; the archive is never run before verification.
$release=Invoke-RestMethod 'https://api.github.com/repos/NVIDIA/NeMo-Speech.cpp/releases/tags/v0.1.0'
$asset=$release.assets | Where-Object name -eq $nemoName
if(!$asset){throw 'Requested NVIDIA release asset unavailable.'}
$nemoUrl=$asset.browser_download_url
$shaText=(Invoke-WebRequest ($nemoUrl+'.sha256') -UseBasicParsing).Content
if($shaText -is [byte[]]){$shaText=[Text.Encoding]::UTF8.GetString($shaText)}
$nemoSha=($shaText.Trim() -split '\s+')[0]
$nemoArchive=Join-Path $downloadDir $nemoName
Get-Verified $nemoUrl $nemoArchive $nemoSha
Expand-Archive -LiteralPath $nemoArchive -DestinationPath (Join-Path $appRoot 'runtimes\nemo') -Force
$llamaArchive=Join-Path $downloadDir 'llama-b10819-bin-win-cpu-x64.zip'
Get-Verified 'https://github.com/ggml-org/llama.cpp/releases/download/b10819/llama-b10819-bin-win-cpu-x64.zip' $llamaArchive '4599e502b374196d24600ea9b03c842a448c853116a15b55e8ba502bdc727b3f'
Expand-Archive -LiteralPath $llamaArchive -DestinationPath (Join-Path $appRoot 'runtimes\llama') -Force
Get-Verified 'https://huggingface.co/nvidia/nemotron-3.5-asr-streaming-0.6b/resolve/1c8deaecc64b91f034d73e08dd8b64625eb3395d/nemotron-3.5-asr-streaming-0.6b.q8_0.gguf' (Join-Path $appRoot 'models\asr.gguf') 'a5c435f294eea8f88ce68dd27b8c3bfea7f777cb2fbba04fcd30eaa555f429ae'
Get-Verified 'https://huggingface.co/Qwen/Qwen3-1.7B-GGUF/resolve/main/Qwen3-1.7B-Q8_0.gguf' (Join-Path $appRoot 'models\editor.gguf') '061b54daade076b5d3362dac252678d17da8c68f07560be70818cace6590cb1a'
Push-Location $appRoot
try {
    & npm.cmd ci --no-audit --no-fund
    if($LASTEXITCODE -ne 0){throw 'App dependency installation failed.'}
    & node node_modules\electron\install.js
    if($LASTEXITCODE -ne 0){throw 'Electron installation failed.'}
    & npm.cmd test
    if($LASTEXITCODE -ne 0){throw 'App checks failed.'}
} finally {Pop-Location}
Write-Host 'Setup complete. Open Launch.cmd, then Local engines > Start local engines. Internet is no longer required.'
