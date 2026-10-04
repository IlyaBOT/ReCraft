# Downloads and verification

Use the platform files in [ReCraft Releases](https://github.com/IlyaBOT/ReCraft/releases).
GitHub Actions artifacts are development outputs: their storage download URLs
are temporary, and the Actions ZIP may contain a second platform archive.
Release download links remain usable without copying a signed storage URL.

On Windows, extract the complete ZIP and launch `ReCraft.exe` inside its folder.
The package includes required non-system DLLs; it does not need MSYS2 installed.
Extract to a writable folder so the client can create its own saves/config.
Linux packages need system OpenAL, zlib, X11 and OpenGL libraries. Vesper macOS
packages target its current SDK, not the Snow Leopard/i386 recipe.

## Checksums

Download `SHA256SUMS.txt` from the same release. Compare the archive hash:

```powershell
Get-FileHash .\ReCraft-0.2.0-dev-windows-x64.zip -Algorithm SHA256
```

On Linux/macOS, from the directory containing all listed downloads:

```sh
sha256sum -c SHA256SUMS.txt    # Linux
shasum -a 256 -c SHA256SUMS.txt # macOS
```

The extracted runtime has another `SHA256SUMS.txt` listing its packaged files
and `BUILD_INFO.txt` identifying the source revision. Checksums detect changes;
they do not establish publisher identity or prove that a program is safe.

## Chrome warning investigation, 4 October 2026

The reported file was the Windows `0.2.0-dev` artifact from
[CI run 37195285116](https://github.com/IlyaBOT/ReCraft/actions/runs/37195285116).
The supplied Azure storage link expired at 11:24:23 UTC and later returned HTTP
403. This explains why that particular URL cannot be reused, not why Chrome
displayed a dangerous-download warning before expiry.

The same CI artifact was retrieved through a fresh GitHub URL and its outer ZIP
SHA-256 matched GitHub's digest:
`63d665d6f06eb97a1eb83cbdf52b67321f1d1632dc6a02fdddc64f69203d3536`.
It contained 454 files, including 440 runtime assets, the executable and five
non-system DLLs, documentation and notices. No saves, account files, config,
reference tree or unrelated output directories were present.
Microsoft Defender definitions `1.459.546.0` reported no threats in both the
original ZIP and the unpacked directory. These checks do not establish which
Google Safe Browsing verdict caused the reported Chrome warning.

The Windows executable is not Authenticode-signed. The macOS app is not Developer
ID-signed/notarized. A new unsigned binary may have limited download reputation,
but that is a possible explanation rather than a confirmed diagnosis. Chrome
distinguishes dangerous, suspicious/uncommon and insecure downloads;
see [Google's explanation](https://support.google.com/chrome/answer/6261569).

Distribution now has permanent Release links, explicit version metadata,
clean staging, an automated Defender scan, exact file inventories and checksum
verification before publication. These changes improve provenance and packaging;
they cannot guarantee that Google clears an existing warning. Do not disable
Safe Browsing to test a release. If a new release is still flagged, retain the
exact warning and file SHA-256 for a provider review. Trusted code signing would
require a real publisher certificate; no self-signed substitute is used.
