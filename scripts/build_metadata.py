Import("env")

from pathlib import Path
import subprocess


def git_value(arguments, default):
    try:
        return subprocess.check_output(
            ["git", *arguments],
            cwd=Path(env["PROJECT_DIR"]),
            text=True,
        ).strip()
    except Exception:
        return default


git_sha = git_value(["rev-parse", "--short", "HEAD"], "unknown")
git_describe = git_value(["describe", "--tags", "--dirty", "--always"], git_sha)
build_env = env["PIOENV"]

env.Append(
    CPPDEFINES=[
        ("BUILD_GIT_SHA", '\\"%s\\"' % git_sha),
        ("BUILD_GIT_DESCRIBE", '\\"%s\\"' % git_describe),
        ("BUILD_ENV_NAME", '\\"%s\\"' % build_env),
    ]
)
