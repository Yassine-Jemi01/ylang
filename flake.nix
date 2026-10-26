{
  description = "YLang — a small compiled programming language";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: import nixpkgs { inherit system; };

      mkYLang = pkgs: pkgs.stdenv.mkDerivation {
        pname = "ylang";
        version = pkgs.lib.trim (builtins.readFile ./VERSION);
        src = self;

        nativeBuildInputs = with pkgs; [
          gnumake
          python3
          makeWrapper
          gcc
          clang
          pkg-config
          nasm
          binutils
        ];

        dontConfigure = true;

        buildPhase = ''
          runHook preBuild
          make CC=${pkgs.stdenv.cc}/bin/cc PREFIX="$out"
          runHook postBuild
        '';

        doCheck = true;
        checkPhase = ''
          runHook preCheck
          make CC=${pkgs.stdenv.cc}/bin/cc PREFIX="$out" test
          runHook postCheck
        '';

        installPhase = ''
          runHook preInstall

          make CC=${pkgs.stdenv.cc}/bin/cc PREFIX="$out" install

          install -d "$out/share/ylang" "$out/include/ylang" "$out/share/doc/ylang"
          cp -R stdlib "$out/share/ylang/stdlib"
          cp -R include/ylang/. "$out/include/ylang/"
          install -m 0644 README.md CHANGELOG.md LICENSE "$out/share/doc/ylang/"

          wrapProgram "$out/bin/ylang" \
            --set YLANG_PATH "$out/share/ylang/stdlib" \
            --prefix PATH : "${pkgs.lib.makeBinPath [
              pkgs.gcc
              pkgs.clang
              pkgs.nasm
              pkgs.binutils
            ]}"

          runHook postInstall
        '';

        meta = {
          description = "A small compiled language with readable syntax and explicit types";
          homepage = "https://github.com/Yassine-Jemi01/ylang";
          license = pkgs.lib.licenses.mit;
          platforms = [ "x86_64-linux" ];
          mainProgram = "ylang";
        };
      };
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = pkgsFor system;
          ylang = mkYLang pkgs;
        in {
          default = ylang;
          ylang = ylang;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/ylang";
        };
      });

      devShells = forAllSystems (system:
        let pkgs = pkgsFor system;
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              gcc
              clang
              gnumake
              python3
              pkg-config
              SDL2
              nasm
              binutils
            ];

            shellHook = ''
              export YLANG_PATH="$PWD/stdlib"
              printf 'YLang development environment ready. Run: make test\n'
            '';
          };
        });
    };
}
