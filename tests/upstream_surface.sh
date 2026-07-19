#!/usr/bin/env bash
set -euo pipefail

MINIZ_SRC="${MINIZ_SRC:-../miniz}"
EXPECTED_COMMIT=77d0dce8627735138c51770d1799a1ef48f2117d

if [[ ! -d "$MINIZ_SRC/.git" ]]; then
    echo "upstream surface audit skipped (MINIZ_SRC checkout unavailable)"
    exit 0
fi

actual_commit=$(git -C "$MINIZ_SRC" rev-parse HEAD)
if [[ "$actual_commit" != "$EXPECTED_COMMIT" ]]; then
    echo "unexpected miniz revision: expected $EXPECTED_COMMIT, got $actual_commit" >&2
    exit 1
fi

actual=$(mktemp)
definitions=$(mktemp)
complete_surface=$(mktemp)
ledger=$(mktemp)
config=$(mktemp)
config_ledger=$(mktemp)
called=$(mktemp)
exercised=$(mktemp)
trap 'rm -f "$actual" "$definitions" "$complete_surface" "$ledger" "$config" "$config_ledger" "$called" "$exercised"' EXIT

perl -0777 -ne '
    while (/MINIZ_EXPORT\s+[^{;]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(/sg) {
        print "$1\n";
    }
' "$MINIZ_SRC/miniz.h" "$MINIZ_SRC/miniz_tdef.h" \
  "$MINIZ_SRC/miniz_tinfl.h" "$MINIZ_SRC/miniz_zip.h" > "$actual"

sed -n 's/.*\(mz_zip_streaming_extract_[A-Za-z0-9_]*\)(.*/\1/p' \
    "$MINIZ_SRC/miniz_zip.h" >> "$actual"

LC_ALL=C sort -u "$actual" -o "$actual"
diff -u tests/upstream_symbols.txt "$actual"

count=$(wc -l < "$actual" | tr -d ' ')
if [[ "$count" != 118 ]]; then
    echo "unexpected public entry-point count: $count" >&2
    exit 1
fi

# Every implemented public entry point must be invoked by its exact upstream
# name in a C differential oracle. The six streaming-extract names are inside
# an upstream #if 0 TODO block and have declarations but no C definitions;
# their native implementation is covered by tests/zip_seek.sx instead.
while IFS= read -r symbol; do
    if rg -q "\\b${symbol}\\s*\\(" tests -g '*.c'; then
        printf '%s\n' "$symbol" >> "$called"
    fi
done < "$actual"
LC_ALL=C sort -u "$called" tests/upstream_declaration_only_symbols.txt > "$exercised"
diff -u "$actual" "$exercised"

called_count=$(wc -l < "$called" | tr -d ' ')
declaration_only_count=$(wc -l < tests/upstream_declaration_only_symbols.txt | tr -d ' ')
if [[ "$called_count" != 112 || "$declaration_only_count" != 6 ]]; then
    echo "public entry-point evidence is incomplete: called=$called_count declaration_only=$declaration_only_count" >&2
    exit 1
fi

# The completion ledger must name every symbol individually, even though its
# human-readable rows group related entry points. This prevents an API from
# remaining in the manifest while silently disappearing from the port plan.
sed -n '/^## Public entry-point ledger/,/^## Internal\/configuration path ledger/p' docs/COVERAGE.md | perl -ne '
    if (/^\| (?:Complete|Partial|Open) \|/) {
        while (/\b((?:mz|tdefl|tinfl)_[A-Za-z0-9_]+)\b/g) { print "$1\n"; }
    }
' | LC_ALL=C sort -u > "$ledger"
diff -u tests/upstream_symbols.txt "$ledger"

ledger_count=$(wc -l < "$ledger" | tr -d ' ')
if [[ "$ledger_count" != "$count" ]]; then
    echo "coverage ledger does not enumerate the full upstream surface: $ledger_count/$count" >&2
    exit 1
fi

# Inventory implementation functions too, including private/static helpers.
# Comments are removed before matching so disabled examples cannot masquerade
# as definitions. The six streaming-extract declarations intentionally have
# no definition in the pinned C source, so union the definition inventory with
# the checked public surface before comparing it to the documentation ledger.
perl -0777 -ne '
    s{/\*.*?\*/}{}gs;
    s{//[^\n]*}{}g;
    while (/^(?:\s*static\s+)?(?:MZ_FORCEINLINE\s+)?(?:MINIZ_EXPORT\s+)?[\w\s\*]+?\b((?:mz|tdefl|tinfl)_[A-Za-z0-9_]+)\s*\([^;{}]*\)\s*\{/mg) {
        print "$1\n";
    }
' "$MINIZ_SRC/miniz.c" "$MINIZ_SRC/miniz_tdef.c" \
  "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_zip.c" | LC_ALL=C sort -u > "$definitions"

definition_count=$(wc -l < "$definitions" | tr -d ' ')
if [[ "$definition_count" != 180 ]]; then
    echo "unexpected implementation-function count: $definition_count" >&2
    exit 1
fi

LC_ALL=C sort -u "$actual" "$definitions" > "$complete_surface"
perl -ne '
    if (/^\| (?:Complete|Partial|Open) \|/) {
        while (/\b((?:mz|tdefl|tinfl)_[A-Za-z0-9_]+)\b/g) { print "$1\n"; }
    }
' docs/COVERAGE.md | LC_ALL=C sort -u > "$ledger"
diff -u "$complete_surface" "$ledger"

complete_count=$(wc -l < "$complete_surface" | tr -d ' ')
implementation_ledger_count=$(wc -l < "$ledger" | tr -d ' ')
if [[ "$complete_count" != 186 || "$implementation_ledger_count" != "$complete_count" ]]; then
    echo "implementation ledger does not enumerate the full source: $implementation_ledger_count/$complete_count" >&2
    exit 1
fi

# Compile-time switches select real source paths or public declaration
# surfaces, so they are part of the port inventory too. Keep even alias and
# platform-detection names: dropping one can otherwise hide a build variant.
rg -o 'MINIZ_[A-Z0-9_]+' \
    "$MINIZ_SRC/miniz.h" "$MINIZ_SRC/miniz.c" \
    "$MINIZ_SRC/miniz_tdef.c" "$MINIZ_SRC/miniz_tinfl.c" \
    "$MINIZ_SRC/miniz_zip.h" "$MINIZ_SRC/miniz_zip.c" |
    sed 's/.*://' | LC_ALL=C sort -u > "$config"
diff -u tests/upstream_config_symbols.txt "$config"

sed -n '/^## Compile-time configuration ledger/,/^## Current proven subset/p' docs/COVERAGE.md |
    rg -o 'MINIZ_[A-Z0-9_]+' | LC_ALL=C sort -u > "$config_ledger"
diff -u tests/upstream_config_symbols.txt "$config_ledger"

config_count=$(wc -l < "$config" | tr -d ' ')
config_ledger_count=$(wc -l < "$config_ledger" | tr -d ' ')
if [[ "$config_count" != 18 || "$config_ledger_count" != "$config_count" ]]; then
    echo "configuration ledger does not enumerate the full source: $config_ledger_count/$config_count" >&2
    exit 1
fi

# Completion is fail-closed. A newly discovered or regressed path must make
# the full-port gate red until its implementation and named evidence are done.
if rg -n '^\| (Partial|Open) \|' docs/COVERAGE.md; then
    echo "coverage ledger contains incomplete rows" >&2
    exit 1
fi

echo "upstream surface: revision=$actual_commit entry_points=$count called=$called_count declaration_only=$declaration_only_count definitions=$definition_count total=$complete_count configs=$config_count ledger=$implementation_ledger_count exact=yes"
