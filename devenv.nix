{ pkgs, config, ... }:

let
  # The Pebble SDK downloads prebuilt binaries at install time (ARM toolchain,
  # QEMU emulator, CPython via uv). Those expect a standard FHS filesystem
  # layout, which NixOS does not provide, so every pebble command runs inside
  # this bubblewrap-based FHS wrapper.
  pebble-fhs = pkgs.buildFHSEnv {
    name = "pebble-fhs";
    targetPkgs =
      pkgs: with pkgs; [
        uv
        nodejs_22
        python313

        # QEMU emulator runtime dependencies
        SDL2
        glib
        pixman
        zlib
        sndio
        libpulseaudio
        libGL
        freetype
        fontconfig
        libpng
        libjpeg
        alsa-lib

        # common runtime libs for the downloaded toolchain/python
        expat
        libffi
        openssl
        ncurses
        util-linux
        libusb1
        stdenv.cc.cc.lib
      ];
    runScript = pkgs.writeShellScript "pebble-fhs-run" ''
      # devenv exports CC/AR/LD/... pointing at the host toolchain; waf gives
      # them priority over the SDK's arm-none-eabi cross compiler, which
      # breaks the firmware build. Clear them inside the FHS environment.
      unset CC CXX LD AR AS RANLIB STRIP OBJCOPY OBJDUMP NM SIZE READELF
      unset NIX_CFLAGS_COMPILE NIX_LDFLAGS
      # The FHS env has no system CA store; Python's ssl (used by the
      # CloudPebble/Dev Connect websocket) needs an explicit bundle.
      export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
      export NIX_SSL_CERT_FILE="$SSL_CERT_FILE"
      if [ $# -eq 0 ]; then
        exec bash
      fi
      exec "$@"
    '';
  };
in
{
  packages = [
    pebble-fhs
    pkgs.git
  ];

  env = {
    # Keep uv-managed tools and Python builds inside the project state dir
    # so the whole toolchain is self-contained and disposable.
    UV_TOOL_DIR = "${config.env.DEVENV_STATE}/uv/tools";
    UV_TOOL_BIN_DIR = "${config.env.DEVENV_STATE}/uv/bin";
    UV_PYTHON_INSTALL_DIR = "${config.env.DEVENV_STATE}/uv/python";
  };

  # `pebble` transparently proxies into the FHS environment.
  scripts.pebble.exec = ''
    if [ ! -x "$UV_TOOL_BIN_DIR/pebble" ]; then
      echo "pebble-tool is not installed yet. Run: pebble-setup" >&2
      exit 1
    fi
    exec pebble-fhs "$UV_TOOL_BIN_DIR/pebble" "$@"
  '';

  # One-shot bootstrap: installs pebble-tool and the SDK core.
  scripts.pebble-setup.exec = ''
    set -euo pipefail
    echo "==> Installing pebble-tool (uv, Python 3.13)..."
    pebble-fhs uv tool install pebble-tool --python 3.13
    echo "==> Installing the Pebble SDK core..."
    pebble-fhs "$UV_TOOL_BIN_DIR/pebble" sdk install latest
    echo "==> Pre-populating STPyV8 ICU data..."
    # STPyV8 (pkjs runtime) tries to write ICU data to /usr/share/stpyv8,
    # which is read-only inside the FHS env, and the EROFS error is not
    # handled upstream. Seeding the user-level fallback dir makes icu_sync()
    # return early before it attempts that write.
    pebble-fhs sh -c '
      PY="$UV_TOOL_DIR/pebble-tool/bin/python"
      VER=$("$PY" -c "import _STPyV8; print(_STPyV8.JSEngine.version)")
      ICU=$(find "$UV_TOOL_DIR/pebble-tool" -name icudtl.dat | head -1)
      mkdir -p ~/.local/share/stpyv8
      cp "$ICU" ~/.local/share/stpyv8/
      printf "%s" "$VER" > ~/.local/share/stpyv8/stpyv8-version.txt
    '
    echo "==> Done. Try: pebble build && pebble install --emulator basalt"
  '';

  enterShell = ''
    if [ ! -x "$UV_TOOL_BIN_DIR/pebble" ]; then
      echo ""
      echo "  Pebble SDK is not bootstrapped yet. Run: pebble-setup"
      echo ""
    fi
  '';
}
