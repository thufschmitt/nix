with import ./config.nix;

{ seed }:
let
  ca = mkDerivation {
    name = "simple-content-addressed";
    builder = ./simple.builder.sh;
    PATH = "";
    goodPath = "${path}:${toString seed}";
    contentAddressed = true;
  };
  dep = mkDerivation {
    name = "content-addressed-dependency";
    buildCommand = "echo \"building dep\" && echo ${ca}/hello > $out";
  };
  dep2 = mkDerivation {
    name = "meta-dependency";
    buildCommand = "cat ${dep} > $out";
  };
in
dep2
