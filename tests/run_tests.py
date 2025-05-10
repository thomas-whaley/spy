from typing import Literal, TypedDict
import subprocess
import os
import json
import pprint
import argparse

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'



REPO_ROOT = subprocess.run(['git', 'rev-parse', '--show-toplevel'], stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.decode().strip()

SpyTarget = Literal["x86-64-macos", "aarch64-mac-m1", "python311", "ir", "lexer"]


class SpyResult(TypedDict):
    input_file: str
    target: SpyTarget
    comp_stdout: str
    comp_stderr: str
    run_stdout: str
    run_stderr: str


def format_result(result: SpyResult) -> str:
    output: list[str] = []
    output.append("{")
    for i, (k, v) in enumerate(result.items()):
        output.append(f"    \"{k}\": {json.dumps(v)}")
        if i != len(result) - 1:
            output[-1] += ","
    output.append("}")
    return os.linesep.join(output)


def run_spy(input_file: str, target: SpyTarget, run: bool = False) -> SpyResult:
    should_run: bool = run and target == "x86-64-macos"
    temp_file_name: str = 'temp_run_file.s'
    args: list[str] = [os.path.join(REPO_ROOT, "build", "spy"), input_file, "-target", target]
    if should_run:
        args.append("-o")
        args.append(temp_file_name)
    comp_result = subprocess.run(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if comp_result.returncode == 0 and should_run:
        exe_name: str = temp_file_name.removesuffix(".s")
        link_result = subprocess.run(
            ["gcc", temp_file_name, "-o", exe_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if link_result.returncode != 0:
            os.remove(temp_file_name)
            return SpyResult(
                input_file=input_file,
                target=target,
                comp_stdout=comp_result.stdout.decode(),
                comp_stderr=comp_result.stderr.decode(),
                run_stdout=link_result.stdout.decode(),
                run_stderr=link_result.stderr.decode(),
            )
        run_result = subprocess.run(
            [f"./{exe_name}"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        os.remove(temp_file_name)
        os.remove(exe_name)
        return SpyResult(
            input_file=input_file,
            target=target,
            comp_stdout=comp_result.stdout.decode(),
            comp_stderr=comp_result.stderr.decode(),
            run_stdout=run_result.stdout.decode(),
            run_stderr=run_result.stderr.decode(),
        )
    return SpyResult(
        input_file=input_file,
        target=target,
        comp_stdout=comp_result.stdout.decode(),
        comp_stderr=comp_result.stderr.decode(),
        run_stdout="",
        run_stderr="",
    )


def write_result(result: SpyResult, file_path: str) -> None:
    with open(file_path, 'w') as f:
        f.write(format_result(result))


def test_spy(input_file: str, target: SpyTarget, write_anyway: bool = False) -> bool:
    print(f"Testing `{bcolors.OKBLUE}{input_file}{bcolors.ENDC}` `{target}`: ", end='')
    result = run_spy(input_file, target, run=True)
    name, _ = os.path.splitext(input_file)
    json_file: str = f"{name}.json"
    formatted_result = format_result(result)
    if os.path.exists(json_file):
        with open(json_file, 'r') as f:
            expected_json = f.read()
        formatted_expected = expected_json
        if formatted_expected != formatted_result:
            if write_anyway:
                print(bcolors.WARNING, "UPDATED", bcolors.ENDC)
                write_result(result, json_file)
                return True
            print(bcolors.FAIL, "FAILED", bcolors.ENDC)
            print("Expected:")
            print(formatted_expected)
            print("Found:")
            print(formatted_result)
            return False
        elif write_anyway:
            print(bcolors.OKBLUE, "NO CHANGES", bcolors.ENDC)
            return True
        print(bcolors.OKGREEN, "PASSED", bcolors.ENDC)
        return True
    print(bcolors.OKBLUE, "CREATED", bcolors.ENDC)
    write_result(result, json_file)
    return True
    


def test_all_in_dir(dir_path: str, target: SpyTarget, write_anyway: bool = False) -> None:
    failed_paths: list[str] = []
    count: int = 0
    for path in os.listdir(dir_path):
        if not path.endswith(".spy"):
            continue
        passed: bool = test_spy(os.path.join(dir_path, path), target, write_anyway)
        if not passed:
            failed_paths.append(os.path.join(dir_path, path))
        count += 1
    print()
    print(f"Tests passed: {count-len(failed_paths)}/{count}")
    print("Failed tests:")
    for path in failed_paths:
        print(f"   -{bcolors.OKBLUE}{path}{bcolors.ENDC}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("dir")
    parser.add_argument("action", choices=["test", "update"])
    parser.add_argument("--target", default="x86-64-macos", choices=["x86-64-macos", "aarch64-mac-m1", "python311", "ir", "lexer"])

    args = parser.parse_args()

    test_all_in_dir(args.dir, args.target, args.action == "update")
