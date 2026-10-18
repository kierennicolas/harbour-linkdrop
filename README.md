<img src="icons/harbour-linkdrop.svg" width="96" alt="LinkDrop icon">

# LinkDrop for Sailfish OS

An **unofficial native Sailfish OS** file-to-HTTPS-link uploader designed for
sending file URLs over IRC (without DCC).

## Install on the phone (Sony Xperia XA2)

1. Download the **`armv7hl`** RPM from the latest
   [release](../../releases/latest) (or from the artifacts of a
   [Build](../../actions/workflows/build.yml) run).
   The XA2 runs a 32-bit Sailfish OS image, so `armv7hl` is the right one;
   `aarch64` is only for 64-bit images on newer Xperias.
2. On the phone: **Settings → Untrusted software → Allow untrusted software**.
3. Open the RPM from the browser's downloads or the Transfers page and tap
   **Install**. With developer mode on you can instead run
   `devel-su pkcon install-local harbour-linkdrop-*.armv7hl.rpm`.

The RPMs are built against Sailfish OS 4.5.0.16 and install on that release
and anything newer, including 5.x.

## Features

- Native `Sailfish.Silica` UI and swipe / pull-down navigation
- Sailfish `FilePickerPage` for picking any **file type** from local storage
- Asynchronous streaming HTTPS multipart upload, with progress and cancellation
- Waits for a successful server reply containing a valid URL before showing success
- Copy the generated URL to Sailfish's clipboard
- 1h, 12h, 24h, and 72h temporary hosting through Litterbox (maximum 1 GB)
- Optional longer-lived Catbox hosting (maximum 200 MB)
- Application Cover shows the current upload status
- Explicit Sailjail permission declarations for Internet and file access

## Use

1. Tap **Choose file** (or pull down to choose a file).
2. Choose the link duration (24 hours is the default).
3. Tap **Upload file** and keep the application running until the server
   returns success. The application may continue uploading while minimized,
   but do not close or suspend it.
4. Tap **Copy link** and paste into IRC.

The app reads files in-place, not into RAM. It does not keep a local upload
history. After canceling an upload, an incomplete server-side object may remain
until the provider cleans it up.

## How it is built and checked

Every push runs [`.github/workflows/build.yml`](.github/workflows/build.yml):

| Job | What it does |
| --- | --- |
| Upload logic tests | Builds `src/uploader.cpp` against desktop Qt 5 and runs `tests/tst_uploader.cpp`: file selection, validation, and full uploads against a local mock server that checks the exact multipart request (fields, UTF-8 file names, bytes) and every error path. Also lints all QML. |
| RPM (armv7hl / aarch64) | Builds the RPM with `mb2` inside the [Sailfish OS Platform SDK image](https://hub.docker.com/r/coderus/sailfishos-platform-sdk), runs Jolla's Harbour RPM validator on it, and (armv7hl) loads every QML file with the real Sailfish Silica and Pickers modules under qemu, failing on any QML error. |
| GitHub release | On a `v*` tag, attaches the RPMs to a release. |

Running the workflow by hand with **online test** ticked also uploads a small
random file to Litterbox (1 hour expiry), downloads it back and compares the
bytes, then checks that cancelling a large upload works.

To cut a release: bump `Version:` in `rpm/harbour-linkdrop.spec`, then either
`git tag v0.1.1 && git push --tags`, or on GitHub go to **Releases → Draft a
new release**, type a new tag such as `v0.1.1`, and publish. Either way the
workflow builds the RPMs and attaches them to that release.

### Building locally

With the [Sailfish SDK](https://docs.sailfishos.org/Develop/Apps/) installed:

```sh
sfdk config target=SailfishOS-4.5.0.16-armv7hl
sfdk build
```

or open `harbour-linkdrop.pro` in Sailfish IDE.

The upload tests only need Qt 5 (`qtbase5-dev` on Debian/Ubuntu):

```sh
mkdir -p tests/build && cd tests/build && qmake ../tests.pro && make && ./tst_uploader
```

## Manual test checklist (on the device)

- Pick a text file with spaces/non-ASCII name; upload; download and compare.
- Pick a binary file; upload; verify bytes against source.
- Confirm 1h/12h/24h/72h temporary mode returns `https://litter.catbox.moe/`.
- Confirm 'Keep' mode returns `https://files.catbox.moe/`.
- Test cancel, offline, server error, and filesize rejection.
- Verify `Clipboard.text` survives changing to the IRC client.
- Check permissions when picking from SD cards and other user folders.
- Check portrait, landscape, different Ambiences, and background Cover.

## License

MIT (see LICENSE). This project is not affiliated with Wormhole, Catbox,
Litterbox or IRCCloud.
