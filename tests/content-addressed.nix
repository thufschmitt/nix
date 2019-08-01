with import ./config.nix;

let
  ca = mkDerivation {
    name = "simple-content-addressed";
    builder = ./simple.builder.sh;
    PATH = "";
    goodPath = "${path}:${toString builtins.currentTime}";
    contentAddressed = true;
  };
in
mkDerivation {
  name = "content-addressed-dependency";
  buildCommand = "echo ${ca}/hello > $out";
}
