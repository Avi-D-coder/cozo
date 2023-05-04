{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    dream2nix.url = "github:nix-community/dream2nix";
    dream2nix.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, dream2nix }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
      buildInputs = (with pkgs; [
        # rocksdb
        zlib
        zstd
        bzip2
        lz4
        snappy
        liburing
        sqlite

        llvm
        clang
        llvmPackages.libclang
        llvmPackages.libcxxClang
      ]);
    in

    dream2nix.lib.makeFlakeOutputs {
      systems = [ "x86_64-linux" ];
      config.projectRoot = ./.;

      source = ./.;

      # `projects` can alternatively be an attrset.
      # `projects` can be omitted if `autoProjects = true` is defined.
      projects = ./projects.toml;

      packageOverrides = {
        # this will apply to your crate
        # crate.my-overrides = { /* ... */ };
        # this will apply to your crate's dependencies

        cozo-bin.my-overrides = {
          # name the override
          # override attributes
          # preBuild = "...";
          # update attributes
          buildInputs = buildInputs;

          LD_LIBRARY_PATH = (pkgs.lib.makeLibraryPath (buildInputs));
          LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
        };

        cozo-bin-deps.my-overrides = {
          # name the override
          # override attributes
          # preBuild = "...";
          # update attributes
          buildInputs = buildInputs;

          LD_LIBRARY_PATH = (pkgs.lib.makeLibraryPath (buildInputs));
          LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";
        };
        "^.*".set-stdenv.override = old: { stdenv = pkgs.clangStdenv; };
      };
    };
}
