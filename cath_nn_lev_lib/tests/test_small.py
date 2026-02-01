import pathlib

import cath_nn_lev


def py_lev(a: str, b: str) -> int:
    n, m = len(a), len(b)
    dp = list(range(m + 1))
    for i in range(1, n + 1):
        prev = dp[0]
        dp[0] = i
        for j in range(1, m + 1):
            cur = dp[j]
            cost = 0 if a[i - 1] == b[j - 1] else 1
            dp[j] = min(dp[j] + 1, dp[j - 1] + 1, prev + cost)
            prev = cur
    return dp[m]


def test_levenshtein_bounded():
    s = b"kitten"
    t = b"sitting"
    assert cath_nn_lev._levenshtein_bounded(s, t, 2) == 3
    assert cath_nn_lev._levenshtein_bounded(s, t, 3) == 3


def test_find_pairs_exact(tmp_path: pathlib.Path):
    fasta = pathlib.Path(__file__).parent / "data" / "toy.fasta"
    pairs_path = tmp_path / "pairs.tsv"
    directed_path = tmp_path / "directed.tsv"

    stats = cath_nn_lev.find_pairs(
        str(fasta),
        str(pairs_path),
        k=2,
        M=10,
        threads=1,
        strict=0,
        write_directed_path=str(directed_path),
    )
    assert stats["n_sequences"] == 5

    seqs = []
    ids = []
    with open(fasta, "r", encoding="utf-8") as f:
        cur = None
        for line in f:
            if line.startswith(">"):
                if cur is not None:
                    seqs.append(cur)
                ids.append(line[1:].strip().split()[0])
                cur = ""
            else:
                cur += line.strip()
        if cur is not None:
            seqs.append(cur)

    min_dists = []
    for i in range(len(seqs)):
        best = None
        for j in range(len(seqs)):
            if i == j:
                continue
            d = py_lev(seqs[i], seqs[j])
            if best is None or d < best:
                best = d
        min_dists.append(best)

    directed = {}
    with open(directed_path, "r", encoding="utf-8") as f:
        for line in f:
            idx, _id, best_idx, _bid, lev, *_ = line.rstrip("\n").split("\t")
            directed[int(idx)] = (int(best_idx), int(lev))

    for i, (_j, lev) in directed.items():
        assert lev == min_dists[i]

    pairs = set()
    with open(pairs_path, "r", encoding="utf-8") as f:
        for line in f:
            _id1, _id2, idx1, idx2, *_rest = line.rstrip("\n").split("\t")
            a = int(idx1)
            b = int(idx2)
            assert a < b
            pairs.add((a, b))

    for i, (j, _lev) in directed.items():
        a, b = (i, j) if i < j else (j, i)
        assert (a, b) in pairs
