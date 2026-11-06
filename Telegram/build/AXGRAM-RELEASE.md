# Building and packaging AxGram Desktop 7.0.13

Fetch the source with its pinned dependencies using `git clone --recurse-submodules` or `git submodule update --init --recursive`. The build uses your own Telegram API ID and hash, supplied to CMake as `TDESKTOP_API_ID` and `TDESKTOP_API_HASH`.

## Linux x86-64

The release used the Telegram Desktop CentOS build image `ghcr.io/telegramdesktop/tdesktop/centos_env@sha256:29657d0ab22dde4f5fe7dff5f294828908a18548ab3f11c362c63b4d49d7dcc1`, with the source mounted at `/usr/src/tdesktop`. Build and install Protobuf v21.9 in that container using CMake with tests and zlib disabled. Pass its install prefix as `CMAKE_PREFIX_PATH` to `Telegram/build/docker/centos_env/build.sh`, together with the API settings, `-DDESKTOP_APP_DISABLE_AUTOUPDATE=ON` and `-DAXGRAM_ENCRYPT_ARCHIVE_DATABASE=OFF`. The release uses the libdispatch revision pinned by the submodule.

Strip the resulting `out/Release/AxGram` executable and run `Telegram/build/package-linux-appimage.sh` with the source directory, stripped executable, executable linuxdeploy AppImage and output directory. The packaged application uses system GTK 3 and FUSE 2.

## Windows x64

Use the Windows dependency layout and Visual Studio toolchain described in the project build documentation. Configure x64 with the API settings, native auto-update disabled and archive database encryption disabled. Build the `Telegram` target in Release and run `Telegram/build/package_windows.ps1` with the binary directory, Inno Setup compiler and `-WithoutUpdater`.
