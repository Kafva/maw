{
    # nix flake --help
    # https://nixos.wiki/wiki/Flakes#Flake_schema
    description = "maw - Music library manager";

    inputs = {
        nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    };

    outputs = {self, nixpkgs}:
    let
        name = "maw";
        # TODO: use forAllSystems = nixpkgs.lib.genAttrs = { "x86_64-linux", "aarch64-linux" }
        system = "x86_64-linux";
        pkgs = import nixpkgs { inherit system; };
        nativeBuildInputs = with pkgs; [
            makeWrapper
            clang
            ffmpeg_7
            libyaml
            # For building FFmpeg
            libtool
            nasm
            pkg-config
        ];
    in
    {
         # Ran with: `nix develop`
         devShells."${system}".default = pkgs.mkShell {
            packages = nativeBuildInputs ++ (with pkgs; [
                ruby
                lldb
            ]);
         };

         packages."${system}" = rec {
            # Ran with: `nix build`
            default = pkgs.stdenv.mkDerivation {
                name = name;
                src = self;

                # Compile time dependencies
                nativeBuildInputs = nativeBuildInputs;
                buildPhase = ''
                   make
                '';
                installPhase = ''

                '';
            };
            "${name}" = default;
         };
    };
}
