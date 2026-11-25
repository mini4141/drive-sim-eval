import os
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="session", autouse=True)
def repo_cwd():
    os.chdir(ROOT)
    os.environ["SIMEVAL_ROOT"] = str(ROOT)


@pytest.fixture(scope="session")
def scenarios(tmp_path_factory, repo_cwd):
    """A tiny batch of generated scenarios shared by all tests."""
    from simeval import scenarios as scen

    out = tmp_path_factory.mktemp("scenarios")
    scen.generate(10, 7, out)
    return out
