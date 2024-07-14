{
    # nix flake --help
    # https://nixos.wiki/wiki/Flakes#Flake_schema
    description = "maw - Declaratively configure mp4 metadata";

    inputs = {
        nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    };

    outputs = {self, nixpkgs}:
    let
        app_name = "maw";

        supportedSystems = [ "x86_64-linux" "aarch64-linux" ];

        # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
        forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

        # Nixpkgs instantiated for supported system types.
        nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
    in
    {
        packages = forAllSystems (system:
            let pkgs = nixpkgsFor.${system};
            in {
                # Default target for `nix build`
                default = pkgs.stdenv.mkDerivation {
                    name = app_name;
                    src = ./.;
                    # Build time dependencies
                    nativeBuildInputs = with pkgs; [
                        makeWrapper
                        git
                        clang
                        ffmpeg_7
                        libyaml
                    ];
                    buildPhase = ''
                       make
                    '';
                    installPhase = ''
                       make PREFIX=$out install
                    '';
                };
                # Point 'maw' to the default target
                "${app_name}" = self.packages.${system}.default;
            }
        );

        # Development shell: use `nix develop -c $SHELL`
        devShells = forAllSystems (system:
            let pkgs = nixpkgsFor.${system};
            in pkgs.mkShell {
                buildInputs =
                    self.packages.${system}.default.nativeBuildInputs
                    ++ (with pkgs; [
                        # For building FFmpeg
                        autoreconfHook
                        libtool
                        nasm
                        pkg-config
                        # Development
                        ruby
                        lldb
                    ]);
        });
    };
}
