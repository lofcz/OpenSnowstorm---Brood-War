import os
import sys
import subprocess
import glob
import argparse
import time

# OpenSnowstorm Determinism Regression Harness
#
# This script scans for paired .rep and .hash files, runs the gfxtest engine
# headlessly in verification mode, and reports any desyncs.

def run_test(bin_path, replay_path, hash_path):
    basename = os.path.basename(replay_path)
    print(f"  {basename:<40}", end=" ", flush=True)
    start_time = time.time()
    try:
        cmd = [
            bin_path,
            "--headless",
            "--verify-hashes", hash_path,
            "--replay", replay_path
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        elapsed = time.time() - start_time
        if result.returncode == 0:
            print(f"[\033[92mPASS\033[0m] ({elapsed:.1f}s)")
            return True, None
        else:
            print(f"[\033[91mFAIL\033[0m] ({elapsed:.1f}s)")
            error_msg = f"Stdout:\n{result.stdout}\nStderr:\n{result.stderr}"
            return False, error_msg
    except subprocess.TimeoutExpired:
        print(f"[\033[93mTIMEOUT\033[0m]")
        return False, "Process timed out after 10 minutes."
    except Exception as e:
        print(f"[\033[91mERROR\033[0m]")
        return False, str(e)

def main():
    parser = argparse.ArgumentParser(description="OpenSnowstorm Regression Harness")
    parser.add_argument("--bin", help="Path to gfxtest binary")
    parser.add_argument("--dir", default=os.path.join("tests", "replays"), help="Directory containing .rep/.hashes pairs")
    args = parser.parse_args()

    bin_path = args.bin
    if not bin_path:
        bin_path = os.path.join("build", "Release", "gfxtest.exe")
        if sys.platform != "win32":
            bin_path = os.path.join("build", "gfxtest")

    if not os.path.exists(bin_path):
        print(f"Error: Binary not found at {bin_path}. Build the project first or use --bin.")
        sys.exit(1)

    replays = glob.glob(os.path.join(args.dir, "**", "*.rep"), recursive=True)
    if not replays:
        print(f"No replays found in {args.dir}. Check your configuration.")
        sys.exit(0)

    print(f"Running determinism check on {len(replays)} replays...")
    failed_tests = []
    
    for rep in sorted(replays):
        name = os.path.splitext(rep)[0]
        hash_file = None
        for candidate in (name + ".hashes", name + ".hash"):
            if os.path.exists(candidate):
                hash_file = candidate
                break
        if hash_file:
            passed, log = run_test(bin_path, rep, hash_file)
            if not passed:
                failed_tests.append((rep, log))
        else:
            print(f"  {os.path.basename(rep):<40} [\033[94mSKIP\033[0m] (No .hashes/.hash)")

    print("-" * 60)
    if not failed_tests:
        print("\033[92mSUCCESS: All tests passed.\033[0m")
        sys.exit(0)
    else:
        print(f"\033[91mFAILURE: {len(failed_tests)} desync(s) detected.\033[0m\n")
        for rep, log in failed_tests:
            print(f"=== Details for {os.path.basename(rep)} ===")
            print(log)
            print("-" * 40)
        sys.exit(1)

if __name__ == "__main__":
    main()
