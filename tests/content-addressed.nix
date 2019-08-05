with import ./config.nix;

let
  ca = mkDerivation {
    name = "simple-content-addressed";
    builder = ./simple.builder.sh;
    PATH = "";
    goodPath = "${path}:${toString builtins.currentTime}";
    contentAddressed = true;
  };
  dep = mkDerivation {
    name = "content-addressed-dependency";
    buildCommand = "cat ${ca}/hello && echo ${ca}/hello > $out";
  };
  /* Building this will fail because `dep` isn't written at the right place in
  the store nor correctly registered in the DB */
  dep2 = mkDerivation {
    name = "meta-dependency";
    buildCommand = "cat ${dep} > $out";
  };
in
dep2
