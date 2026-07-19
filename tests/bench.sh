#!/usr/bin/env bash
set -euo pipefail

SX_BIN="${SX_BIN:-sx}"
MINIZ_SRC="${MINIZ_SRC:-../miniz}"
CC="${CC:-clang}"
BENCH_CPU="${BENCH_CPU:-generic}"
BENCH_TRIALS="${BENCH_TRIALS:-5}"
BENCH_BASELINE_REF="${BENCH_BASELINE_REF:-}"
BENCH_ENFORCE_BASELINE="${BENCH_ENFORCE_BASELINE:-0}"
BASELINE_NOISE_PERCENT=1
BASELINE_NOISE_NS=10000
BENCHMARK_MARKDOWN="${BENCHMARK_MARKDOWN:-docs/BENCHMARK_RESULTS.md}"
SX_BENCH=/tmp/sx-miniz-bench
SX_DECODE_BENCH=/tmp/sx-miniz-decode-bench
C_BENCH=/tmp/miniz-c-bench
TRIAL_DIR=$(mktemp -d "${TMPDIR:-/tmp}/sx-miniz-bench-trials.XXXXXX")
BASELINE_BENCH="$TRIAL_DIR/sx-baseline-bench"
BASELINE_DECODE_BENCH="$TRIAL_DIR/sx-baseline-decode-bench"
SX_LOG=$(mktemp "${TMPDIR:-/tmp}/sx-miniz-bench-sx.XXXXXX")
C_LOG=$(mktemp "${TMPDIR:-/tmp}/sx-miniz-bench-c.XXXXXX")
BASELINE_LOG=$(mktemp "${TMPDIR:-/tmp}/sx-miniz-bench-baseline.XXXXXX")
IDENTITY_LOG=$(mktemp "${TMPDIR:-/tmp}/sx-miniz-bench-identity.XXXXXX")
trap 'rm -f "$SX_BENCH" "$SX_DECODE_BENCH" "$C_BENCH" "$SX_LOG" "$C_LOG" "$BASELINE_LOG" "$IDENTITY_LOG" /tmp/sx-miniz-bench-{sx,c}-{repeat,random}-{1,6,9}.zlib; rm -rf "$TRIAL_DIR"' EXIT

if ! [[ "$BENCH_TRIALS" =~ ^[1-9][0-9]*$ ]] || (( BENCH_TRIALS % 2 == 0 )); then
    echo "BENCH_TRIALS must be a positive odd integer" >&2
    exit 2
fi
if [[ "$BENCH_ENFORCE_BASELINE" != 0 && "$BENCH_ENFORCE_BASELINE" != 1 ]]; then
    echo "BENCH_ENFORCE_BASELINE must be 0 or 1" >&2
    exit 2
fi

metric() {
    local log=$1 corpus=$2 level=$3 key=$4
    awk -v corpus="$corpus" -v level="$level" -v key="$key" '
        $1 == corpus {
            matched_level = 0
            for (i = 2; i <= NF; ++i) {
                split($i, pair, "=")
                if (pair[1] == "level" && pair[2] == level) matched_level = 1
            }
            if (matched_level) {
                for (i = 2; i <= NF; ++i) {
                    split($i, pair, "=")
                    if (pair[1] == key) { print pair[2]; exit }
                }
            }
        }
    ' "$log"
}

median() {
    printf '%s\n' "$@" | sort -n | awk '{ values[NR] = $1 } END { print values[(NR + 1) / 2] }'
}

aggregate_logs() {
    local output=$1
    shift
    : > "$output"
    local first=$1 corpus level input packed ratio enc dec
    local -a enc_values dec_values
    for corpus in repetitive incompressible; do
        for level in 1 6 9; do
            input=$(metric "$first" "$corpus" "$level" input)
            packed=$(metric "$first" "$corpus" "$level" packed)
            ratio=$(metric "$first" "$corpus" "$level" ratio_permille)
            enc_values=()
            dec_values=()
            for log in "$@"; do
                enc_values+=("$(metric "$log" "$corpus" "$level" encode_ns)")
                dec_values+=("$(metric "$log" "$corpus" "$level" decode_ns)")
            done
            enc=$(median "${enc_values[@]}")
            dec=$(median "${dec_values[@]}")
            printf '%s level=%s input=%s packed=%s ratio_permille=%s encode_ns=%s decode_ns=%s encode_KiB_s=%s decode_KiB_s=%s\n' \
                "$corpus" "$level" "$input" "$packed" "$ratio" "$enc" "$dec" \
                "$((input * 1000000000 / 1024 / (enc > 0 ? enc : 1)))" \
                "$((input * 1000000000 / 1024 / (dec > 0 ? dec : 1)))" >> "$output"
        done
    done
}

run_sx_bench() {
    local encode_bench=$1 decode_bench=$2 output=$3
    "$encode_bench" > "$output"
    "$decode_bench" >> "$output"
}

speedup() {
    awk -v c_ns="$1" -v sx_ns="$2" 'BEGIN {
        if (sx_ns <= 0 || c_ns <= 0) { print "n/a"; exit }
        printf "%.3fx", c_ns / sx_ns
    }'
}

baseline_change() {
    awk -v baseline_ns="$1" -v current_ns="$2" 'BEGIN {
        if (baseline_ns <= 0 || current_ns <= 0) { print "n/a"; exit }
        printf "%+.2f%%", 100 * (current_ns - baseline_ns) / baseline_ns
    }'
}

paired_baseline_change_ppm() {
    local corpus=$1 level=$2 key=$3 pair current_ns baseline_ns
    local -a changes=()
    for ((pair = 0; pair < ${#sx_trials[@]}; ++pair)); do
        current_ns=$(metric "${sx_trials[$pair]}" "$corpus" "$level" "$key")
        baseline_ns=$(metric "${baseline_trials[$pair]}" "$corpus" "$level" "$key")
        changes+=("$(((current_ns - baseline_ns) * 1000000 / baseline_ns))")
    done
    median "${changes[@]}"
}

paired_baseline_change() {
    awk -v change_ppm="$1" 'BEGIN { printf "%+.2f%%", change_ppm / 10000 }'
}

paired_baseline_delta_ns() {
    local corpus=$1 level=$2 key=$3 pair current_ns baseline_ns
    local -a deltas=()
    for ((pair = 0; pair < ${#sx_trials[@]}; ++pair)); do
        current_ns=$(metric "${sx_trials[$pair]}" "$corpus" "$level" "$key")
        baseline_ns=$(metric "${baseline_trials[$pair]}" "$corpus" "$level" "$key")
        deltas+=("$((current_ns - baseline_ns))")
    done
    median "${deltas[@]}"
}

paired_baseline_status() {
    if (( $1 <= 0 || $2 <= 0 )); then
        printf 'PASS'
    elif (( $1 <= BASELINE_NOISE_PERCENT * 10000 || $2 <= BASELINE_NOISE_NS )); then
        printf 'NOISE'
    else
        printf 'REGRESSION'
    fi
}

count_baseline_regressions() {
    local regressions=0 corpus level key paired_change_ppm paired_delta_ns
    for corpus in repetitive incompressible; do
        for level in 1 6 9; do
            for key in encode_ns decode_ns; do
                paired_change_ppm=$(paired_baseline_change_ppm "$corpus" "$level" "$key")
                paired_delta_ns=$(paired_baseline_delta_ns "$corpus" "$level" "$key")
                if (( paired_change_ppm > BASELINE_NOISE_PERCENT * 10000 && paired_delta_ns > BASELINE_NOISE_NS )); then
                    regressions=$((regressions + 1))
                fi
            done
        done
    done
    printf '%s' "$regressions"
}

paired_total_delta_ns() {
    local pair corpus level key current_ns baseline_ns total_delta
    local -a deltas=()
    for ((pair = 0; pair < ${#sx_trials[@]}; ++pair)); do
        total_delta=0
        for corpus in repetitive incompressible; do
            for level in 1 6 9; do
                for key in encode_ns decode_ns; do
                    current_ns=$(metric "${sx_trials[$pair]}" "$corpus" "$level" "$key")
                    baseline_ns=$(metric "${baseline_trials[$pair]}" "$corpus" "$level" "$key")
                    total_delta=$((total_delta + current_ns - baseline_ns))
                done
            done
        done
        deltas+=("$total_delta")
    done
    median "${deltas[@]}"
}

paired_total_change_ppm() {
    local pair corpus level key current_ns baseline_ns total_current total_baseline
    local -a changes=()
    for ((pair = 0; pair < ${#sx_trials[@]}; ++pair)); do
        total_current=0
        total_baseline=0
        for corpus in repetitive incompressible; do
            for level in 1 6 9; do
                for key in encode_ns decode_ns; do
                    current_ns=$(metric "${sx_trials[$pair]}" "$corpus" "$level" "$key")
                    baseline_ns=$(metric "${baseline_trials[$pair]}" "$corpus" "$level" "$key")
                    total_current=$((total_current + current_ns))
                    total_baseline=$((total_baseline + baseline_ns))
                done
            done
        done
        changes+=("$(((total_current - total_baseline) * 1000000 / total_baseline))")
    done
    median "${changes[@]}"
}

paired_gross_delta_ns() {
    local direction=$1 corpus level key delta total=0
    for corpus in repetitive incompressible; do
        for level in 1 6 9; do
            for key in encode_ns decode_ns; do
                delta=$(paired_baseline_delta_ns "$corpus" "$level" "$key")
                if [[ "$direction" == gain && "$delta" -lt 0 ]]; then
                    total=$((total - delta))
                elif [[ "$direction" == loss && "$delta" -gt 0 ]]; then
                    total=$((total + delta))
                fi
            done
        done
    done
    printf '%s' "$total"
}

baseline_gate_status() {
    local regressions=$1 total_change_ppm=$2 total_delta_ns=$3
    if (( regressions == 0 )); then
        printf 'PASS'
    elif (( total_change_ppm < -BASELINE_NOISE_PERCENT * 10000 && total_delta_ns < -BASELINE_NOISE_NS )); then
        printf 'PASS'
    else
        printf 'FAIL'
    fi
}

write_markdown() {
    local c_available=$1 identity_ok=$2
    local report_tmp="${BENCHMARK_MARKDOWN}.tmp.$$"
    mkdir -p "$(dirname "$BENCHMARK_MARKDOWN")"
    {
        echo "# Latest SX vs miniz C benchmark"
        echo
        echo "> Generated by \`tests/bench.sh\` on $(date -u '+%Y-%m-%d %H:%M:%S UTC'). Do not edit measured values by hand."
        echo
        echo "- CPU target: \`$BENCH_CPU\` for both compilers"
        echo "- SX: \`--opt 3\`"
        if [[ "$baseline_available" -eq 1 ]]; then
            echo "- Accepted SX baseline: \`$BENCH_BASELINE_REF\` (built and measured in this run)"
        fi
        if [[ "$c_available" -eq 1 ]]; then
            echo "- C: \`$CC -O3\`"
            if [[ "$identity_ok" -eq 1 ]]; then
                echo "- Byte identity gate: **PASS** (full-file comparison)"
            else
                echo "- Byte identity gate: **FAIL** (full-file comparison)"
            fi
        else
            echo "- C comparison: unavailable"
        fi
        echo "- Timing unit: nanoseconds per operation; each process trial is the median of 15 individually timed operations after 3 warmups, and the reported value is the median of $BENCH_TRIALS process trials"
        echo "- SX encode and decode use separate executables so changes in one codec cannot shift the other benchmark loop's code layout"
        echo

        if [[ "$c_available" -eq 1 ]]; then
            echo "## Latency and exact output"
            echo
            echo "Speedup is \`C time / SX time\`; values above 1.000x mean SX is faster."
            echo
            echo "| Corpus | Level | Input bytes | Packed bytes | Identical | SX encode ns | C encode ns | Encode speedup | SX decode ns | C decode ns | Decode speedup |"
            echo "|---|---:|---:|---:|:---:|---:|---:|---:|---:|---:|---:|"
            for corpus in repetitive incompressible; do
                for level in 1 6 9; do
                    sx_input=$(metric "$SX_LOG" "$corpus" "$level" input)
                    sx_packed=$(metric "$SX_LOG" "$corpus" "$level" packed)
                    sx_enc=$(metric "$SX_LOG" "$corpus" "$level" encode_ns)
                    sx_dec=$(metric "$SX_LOG" "$corpus" "$level" decode_ns)
                    c_enc=$(metric "$C_LOG" "$corpus" "$level" encode_ns)
                    c_dec=$(metric "$C_LOG" "$corpus" "$level" decode_ns)
                    if [[ "$corpus" == repetitive ]]; then identity_corpus=repeat; else identity_corpus=random; fi
                    identity=$(awk -v corpus="$identity_corpus" -v level="$level" '$1 == corpus && $2 == "level=" level { split($3, p, "="); print p[2]; exit }' "$IDENTITY_LOG")
                    printf '| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
                        "$corpus" "$level" "$sx_input" "$sx_packed" "$identity" \
                        "$sx_enc" "$c_enc" "$(speedup "$c_enc" "$sx_enc")" \
                        "$sx_dec" "$c_dec" "$(speedup "$c_dec" "$sx_dec")"
                done
            done
            if [[ "$baseline_available" -eq 1 ]]; then
                echo
                echo "## Accepted baseline comparison"
                echo
                baseline_regressions=$(count_baseline_regressions)
                total_delta_ns=$(paired_total_delta_ns)
                total_change_ppm=$(paired_total_change_ppm)
                gross_gain_ns=$(paired_gross_delta_ns gain)
                gross_loss_ns=$(paired_gross_delta_ns loss)
                tradeoff_status=$(baseline_gate_status "$baseline_regressions" "$total_change_ppm" "$total_delta_ns")
                echo "Current and baseline are paired within each rotated process-trial block. A row is a regression only when the median paired slowdown exceeds both ${BASELINE_NOISE_PERCENT}% and ${BASELINE_NOISE_NS} ns; smaller increases are marked as measurement noise. Individual regressions remain visible, but they fail the gate only when the paired total is not a meaningful net improvement."
                echo
                echo "- Tradeoff gate: **${tradeoff_status}**"
                echo "- Paired total delta: ${total_delta_ns} ns ($(paired_baseline_change "$total_change_ppm"))"
                echo "- Gross row gains: ${gross_gain_ns} ns"
                echo "- Gross row losses: ${gross_loss_ns} ns"
                echo "- Individually significant regressions: ${baseline_regressions}"
                echo
                echo "| Corpus | Level | Metric | Baseline SX ns | Current SX ns | Aggregate change | Paired median delta ns | Paired median change | Status |"
                echo "|---|---:|---|---:|---:|---:|---:|---:|:---:|"
                for corpus in repetitive incompressible; do
                    for level in 1 6 9; do
                        for metric_name in encode decode; do
                            baseline_ns=$(metric "$BASELINE_LOG" "$corpus" "$level" "${metric_name}_ns")
                            current_ns=$(metric "$SX_LOG" "$corpus" "$level" "${metric_name}_ns")
                            paired_change_ppm=$(paired_baseline_change_ppm "$corpus" "$level" "${metric_name}_ns")
                            paired_delta_ns=$(paired_baseline_delta_ns "$corpus" "$level" "${metric_name}_ns")
                            printf '| %s | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
                                "$corpus" "$level" "$metric_name" "$baseline_ns" "$current_ns" \
                                "$(baseline_change "$baseline_ns" "$current_ns")" \
                                "$paired_delta_ns" \
                                "$(paired_baseline_change "$paired_change_ppm")" \
                                "$(paired_baseline_status "$paired_change_ppm" "$paired_delta_ns")"
                        done
                    done
                done
            fi
            echo
            echo "## Throughput"
            echo
            echo "| Corpus | Level | SX encode KiB/s | C encode KiB/s | SX decode KiB/s | C decode KiB/s |"
            echo "|---|---:|---:|---:|---:|---:|"
            for corpus in repetitive incompressible; do
                for level in 1 6 9; do
                    printf '| %s | %s | %s | %s | %s | %s |\n' \
                        "$corpus" "$level" \
                        "$(metric "$SX_LOG" "$corpus" "$level" encode_KiB_s)" \
                        "$(metric "$C_LOG" "$corpus" "$level" encode_KiB_s)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" decode_KiB_s)" \
                        "$(metric "$C_LOG" "$corpus" "$level" decode_KiB_s)"
                done
            done
        else
            echo "## SX results"
            echo
            echo "| Corpus | Level | Input bytes | Packed bytes | Ratio permille | Encode ns | Decode ns | Encode KiB/s | Decode KiB/s |"
            echo "|---|---:|---:|---:|---:|---:|---:|---:|---:|"
            for corpus in repetitive incompressible; do
                for level in 1 6 9; do
                    printf '| %s | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
                        "$corpus" "$level" \
                        "$(metric "$SX_LOG" "$corpus" "$level" input)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" packed)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" ratio_permille)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" encode_ns)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" decode_ns)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" encode_KiB_s)" \
                        "$(metric "$SX_LOG" "$corpus" "$level" decode_KiB_s)"
                done
            done
        fi

        echo
        echo "## Raw output"
        echo
        echo "<details><summary>SX</summary>"
        echo
        echo '```text'
        cat "$SX_LOG"
        echo '```'
        echo "</details>"
        if [[ "$c_available" -eq 1 ]]; then
            echo
            echo "<details><summary>miniz C</summary>"
            echo
            echo '```text'
            cat "$C_LOG"
            echo '```'
            echo "</details>"
        fi
        if [[ "$baseline_available" -eq 1 ]]; then
            echo
            echo "<details><summary>accepted SX baseline ($BENCH_BASELINE_REF)</summary>"
            echo
            echo '```text'
            cat "$BASELINE_LOG"
            echo '```'
            echo "</details>"
        fi
    } > "$report_tmp"
    mv "$report_tmp" "$BENCHMARK_MARKDOWN"
    echo "markdown report: $BENCHMARK_MARKDOWN"
}

"$SX_BIN" build --opt 3 --cpu "$BENCH_CPU" -o "$SX_BENCH" tests/bench.sx
"$SX_BIN" build --opt 3 --cpu "$BENCH_CPU" -o "$SX_DECODE_BENCH" tests/bench_decode.sx
baseline_available=0
if [[ -n "$BENCH_BASELINE_REF" ]]; then
    baseline_tree="$TRIAL_DIR/baseline"
    mkdir -p "$baseline_tree"
    git archive "$BENCH_BASELINE_REF" | tar -x -C "$baseline_tree"
    cp tests/bench.sx tests/bench_decode.sx "$baseline_tree/tests/"
    "$SX_BIN" build --opt 3 --cpu "$BENCH_CPU" -o "$BASELINE_BENCH" "$baseline_tree/tests/bench.sx"
    "$SX_BIN" build --opt 3 --cpu "$BENCH_CPU" -o "$BASELINE_DECODE_BENCH" "$baseline_tree/tests/bench_decode.sx"
    baseline_available=1
fi

c_available=0
identity_ok=1
if command -v "$CC" >/dev/null 2>&1 &&
   [[ -f "$MINIZ_SRC/miniz.c" && -f "$MINIZ_SRC/miniz_tinfl.c" && -f "$MINIZ_SRC/miniz_tdef.c" ]]; then
    common_c_args=(-std=c99 -O3 -DNDEBUG
        -DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=1
        -DMINIZ_LITTLE_ENDIAN=1 -DMINIZ_HAS_64BIT_REGISTERS=1
        -Itests -I"$MINIZ_SRC")
    case "$(uname -m)" in
        arm64|aarch64)
            "$CC" "${common_c_args[@]}" "-mcpu=$BENCH_CPU" \
                tests/bench_miniz.c "$MINIZ_SRC/miniz.c" \
                "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
                -o "$C_BENCH"
            ;;
        x86_64|amd64)
            c_arch="$BENCH_CPU"
            if [[ "$c_arch" == generic ]]; then c_arch=x86-64; fi
            "$CC" "${common_c_args[@]}" "-march=$c_arch" \
                tests/bench_miniz.c "$MINIZ_SRC/miniz.c" \
                "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
                -o "$C_BENCH"
            ;;
        *)
            "$CC" "${common_c_args[@]}" \
                tests/bench_miniz.c "$MINIZ_SRC/miniz.c" \
                "$MINIZ_SRC/miniz_tinfl.c" "$MINIZ_SRC/miniz_tdef.c" \
                -o "$C_BENCH"
            ;;
    esac
    c_available=1
else
    echo "upstream miniz C benchmark skipped (compiler or MINIZ_SRC unavailable)"
fi

echo "process trials (rotating execution order)"
sx_trials=()
c_trials=()
baseline_trials=()
for ((trial = 1; trial <= BENCH_TRIALS; ++trial)); do
    sx_trial_log="$TRIAL_DIR/sx-$trial.log"
    c_trial_log="$TRIAL_DIR/c-$trial.log"
    baseline_trial_log="$TRIAL_DIR/baseline-$trial.log"
    if [[ "$baseline_available" -eq 1 && "$c_available" -eq 1 ]]; then
        case $((trial % 3)) in
            1)
                run_sx_bench "$BASELINE_BENCH" "$BASELINE_DECODE_BENCH" "$baseline_trial_log"
                run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
                "$C_BENCH" > "$c_trial_log"
                ;;
            2)
                run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
                "$C_BENCH" > "$c_trial_log"
                run_sx_bench "$BASELINE_BENCH" "$BASELINE_DECODE_BENCH" "$baseline_trial_log"
                ;;
            0)
                "$C_BENCH" > "$c_trial_log"
                run_sx_bench "$BASELINE_BENCH" "$BASELINE_DECODE_BENCH" "$baseline_trial_log"
                run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
                ;;
        esac
    elif [[ "$baseline_available" -eq 1 ]]; then
        if (( trial % 2 == 0 )); then
            run_sx_bench "$BASELINE_BENCH" "$BASELINE_DECODE_BENCH" "$baseline_trial_log"
            run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
        else
            run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
            run_sx_bench "$BASELINE_BENCH" "$BASELINE_DECODE_BENCH" "$baseline_trial_log"
        fi
    elif [[ "$c_available" -eq 1 && $((trial % 2)) -eq 0 ]]; then
        "$C_BENCH" > "$c_trial_log"
        run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
    else
        run_sx_bench "$SX_BENCH" "$SX_DECODE_BENCH" "$sx_trial_log"
        if [[ "$c_available" -eq 1 ]]; then "$C_BENCH" > "$c_trial_log"; fi
    fi
    sx_trials+=("$sx_trial_log")
    if [[ "$c_available" -eq 1 ]]; then c_trials+=("$c_trial_log"); fi
    if [[ "$baseline_available" -eq 1 ]]; then baseline_trials+=("$baseline_trial_log"); fi
    echo "  process trial $trial/$BENCH_TRIALS"
done

echo "sx (--opt 3 --cpu $BENCH_CPU)"
aggregate_logs "$SX_LOG" "${sx_trials[@]}"
cat "$SX_LOG"

if [[ "$baseline_available" -eq 1 ]]; then
    echo "accepted SX baseline ($BENCH_BASELINE_REF, --opt 3 --cpu $BENCH_CPU)"
    aggregate_logs "$BASELINE_LOG" "${baseline_trials[@]}"
    cat "$BASELINE_LOG"
fi

if [[ "$c_available" -eq 1 ]]; then
    echo "upstream miniz C (-O3 cpu=$BENCH_CPU)"
    aggregate_logs "$C_LOG" "${c_trials[@]}"
    cat "$C_LOG"
    # A baseline trial may have written the fixed SX artifact paths last.
    # Refresh them with the current binary before the byte-identity gate.
    "$SX_BENCH" >/dev/null
    echo "byte identity"
    for corpus in repeat random; do
        for level in 1 6 9; do
            if cmp -s "/tmp/sx-miniz-bench-sx-$corpus-$level.zlib" "/tmp/sx-miniz-bench-c-$corpus-$level.zlib"; then
                identity_line="$corpus level=$level identical=yes"
            else
                first=$(cmp -l "/tmp/sx-miniz-bench-sx-$corpus-$level.zlib" "/tmp/sx-miniz-bench-c-$corpus-$level.zlib" 2>/dev/null | head -n 1 || true)
                identity_line="$corpus level=$level identical=no first_difference=$first"
                identity_ok=0
            fi
            echo "$identity_line" | tee -a "$IDENTITY_LOG"
        done
    done
fi

write_markdown "$c_available" "$identity_ok"
if [[ "$identity_ok" -ne 1 ]]; then
    exit 1
fi
if [[ "$baseline_available" -eq 1 ]]; then
    baseline_regressions=$(count_baseline_regressions)
    total_delta_ns=$(paired_total_delta_ns)
    total_change_ppm=$(paired_total_change_ppm)
    tradeoff_status=$(baseline_gate_status "$baseline_regressions" "$total_change_ppm" "$total_delta_ns")
    echo "accepted baseline regressions: $baseline_regressions"
    echo "accepted baseline paired total: ${total_delta_ns} ns ($(paired_baseline_change "$total_change_ppm")); tradeoff gate: $tradeoff_status"
    if [[ "$BENCH_ENFORCE_BASELINE" -eq 1 && "$tradeoff_status" != PASS ]]; then
        exit 1
    fi
fi
