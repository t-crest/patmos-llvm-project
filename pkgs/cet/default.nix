{
  pkgs,
  system,
  patmos-prefixed,
  patmos-simulator,
  # LLVM's own lit (llvm/utils/lit/lit.py from the source tree): the CET
  # suite's lit.cfg.py imports lit.llvm modules, which only LLVM's bundled
  # lit provides.
  llvm-lit,
}: let
  # Same CET suite pin as the CI workflow uses.
  cetSrc = pkgs.fetchFromGitHub {
    owner = "t-crest";
    repo = "patmos-cet-test-suite";
    rev = "82a96fa3f13ce8e916bc56872c10bfa5b00652de";
    sha256 = "17sc9zb8bj4ly37mm18r3dn8jjip5bfgaqaqqyb6nd2h7lsqa756";
  };
in {
  # Status check, NOT a gate. Deliberately kept out of checks and out of
  # the tarball gate: this derivation may fail, and the CI step running
  # it carries continue-on-error, so failures show as a red step while
  # testing and packaging stay green.
  packages.patmos-cet-status = pkgs.stdenv.mkDerivation {
    name = "patmos-cet-status-${system}";
    src = cetSrc;
    nativeBuildInputs = [patmos-prefixed patmos-simulator pkgs.python3];

    buildPhase = ''
      # assert_correct.py needs patmos-clang and plain pasim on PATH;
      # RUN lines invoke python3 themselves.
      export PATH="${patmos-prefixed}/bin:${patmos-simulator}/bin:$PATH"
      python3 ${llvm-lit} . -v \
        --filter="^(?!((.*/dijkstra.c)|(.*/gsm_dec.c)|(.*/huff_dec.c)|(.*/st.c)|(.*/minver.c)|(.*/cosf.c))).*" \
        2>&1 | tee patmos-cet-tests.log
    '';

    installPhase = ''
      mkdir -p $out
      cp patmos-cet-tests.log "$out/"
    '';
  };
}
