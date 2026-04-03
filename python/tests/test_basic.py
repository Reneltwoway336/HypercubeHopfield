"""Smoke tests for the hypercube_hopfield Python SDK."""

import pickle
import numpy as np
import pytest

import hypercube_hopfield as hh
from hypercube_hopfield import HopfieldNetwork, RecallResult, UpdateMode


# ── Helpers ──

def random_pattern(n, rng):
    return rng.standard_normal(n).astype(np.float32)


def overlap(a, b):
    """Cosine similarity."""
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    if na < 1e-12 or nb < 1e-12:
        return 0.0
    return float(np.dot(a, b) / (na * nb))


# ── Tests ──

class TestConstruction:
    @pytest.mark.parametrize("dim", range(4, 17))
    def test_all_dims(self, dim):
        net = HopfieldNetwork(dim=dim, seed=1)
        assert net.dim == dim
        assert net.num_vertices == 2**dim
        assert net.num_patterns == 0

    def test_invalid_dim_low(self):
        with pytest.raises(ValueError, match="dim must be"):
            HopfieldNetwork(dim=3)

    def test_invalid_dim_high(self):
        with pytest.raises(ValueError, match="dim must be"):
            HopfieldNetwork(dim=17)

    def test_invalid_dim_float(self):
        with pytest.raises(ValueError, match="dim must be"):
            HopfieldNetwork(dim=4.5)

    def test_invalid_dim_string(self):
        with pytest.raises(ValueError, match="dim must be"):
            HopfieldNetwork(dim="8")

    def test_defaults(self):
        net = HopfieldNetwork(dim=6)
        assert net.reach == 3  # dim // 2
        assert net.beta == pytest.approx(4.0)
        assert net.neighbor_fraction == pytest.approx(1.0)
        assert net.tolerance == pytest.approx(1e-6)
        assert net.seed == 0

    def test_custom_config(self):
        net = HopfieldNetwork(
            dim=8, seed=99, reach=2, beta=2.5,
            neighbor_fraction=0.5, tolerance=1e-4,
        )
        assert net.dim == 8
        assert net.seed == 99
        assert net.reach == 2
        assert net.beta == pytest.approx(2.5)
        assert net.neighbor_fraction == pytest.approx(0.5)
        assert net.tolerance == pytest.approx(1e-4)

    def test_repr(self):
        net = HopfieldNetwork(dim=6, seed=1)
        r = repr(net)
        assert "dim=6" in r
        assert "N=64" in r
        assert "patterns=0" in r
        assert "reach=3" in r
        assert "beta=4.0" in r

    def test_invalid_beta(self):
        with pytest.raises(ValueError, match="beta"):
            HopfieldNetwork(dim=6, beta=-1.0)

    def test_invalid_neighbor_fraction_zero(self):
        with pytest.raises(ValueError, match="neighbor_fraction"):
            HopfieldNetwork(dim=6, neighbor_fraction=0.0)

    def test_invalid_neighbor_fraction_high(self):
        with pytest.raises(ValueError, match="neighbor_fraction"):
            HopfieldNetwork(dim=6, neighbor_fraction=1.5)

    def test_invalid_tolerance(self):
        with pytest.raises(ValueError, match="tolerance"):
            HopfieldNetwork(dim=6, tolerance=-1e-6)

    def test_negative_reach(self):
        with pytest.raises(ValueError, match="reach"):
            HopfieldNetwork(dim=6, reach=-1)

    def test_negative_seed(self):
        with pytest.raises(ValueError, match="seed"):
            HopfieldNetwork(dim=6, seed=-1)


class TestStoreAndRecall:
    def test_store_single(self):
        net = HopfieldNetwork(dim=6, seed=1)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        assert net.num_patterns == 1

    def test_store_multiple(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        for _ in range(5):
            net.store_pattern(random_pattern(64, rng))
        assert net.num_patterns == 5

    def test_recall_perfect(self):
        net = HopfieldNetwork(dim=6, seed=1)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        result = net.recall(pat)
        assert result.converged
        assert overlap(result.state, pat) > 0.99

    def test_recall_noisy(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        pat = random_pattern(64, rng)
        net.store_pattern(pat)
        noise = rng.standard_normal(64).astype(np.float32) * 0.3
        result = net.recall(pat + noise)
        ov = overlap(result.state, pat)
        assert ov > 0.90, f"overlap {ov} too low"

    def test_recall_returns_named_tuple(self):
        net = HopfieldNetwork(dim=6, seed=1)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        result = net.recall(pat)
        assert isinstance(result, RecallResult)
        assert isinstance(result.state, np.ndarray)
        assert isinstance(result.steps, int)
        assert isinstance(result.converged, bool)
        # Unpacking still works
        state, steps, converged = result
        assert state is result.state

    def test_recall_does_not_mutate_input(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        pat = random_pattern(64, rng)
        net.store_pattern(pat)
        cue = pat + rng.standard_normal(64).astype(np.float32) * 0.5
        cue_copy = cue.copy()
        net.recall(cue)
        np.testing.assert_array_equal(cue, cue_copy)

    def test_recall_async(self):
        net = HopfieldNetwork(dim=6, seed=1)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        result = net.recall(pat, mode=UpdateMode.Async)
        assert overlap(result.state, pat) > 0.99

    def test_recall_no_patterns(self):
        net = HopfieldNetwork(dim=6, seed=1)
        cue = random_pattern(64, np.random.default_rng(42))
        result = net.recall(cue)
        assert result.steps == 0

    def test_recall_max_steps_zero(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(random_pattern(64, np.random.default_rng(42)))
        result = net.recall(
            random_pattern(64, np.random.default_rng(99)), max_steps=0,
        )
        assert result.steps == 0

    def test_recall_2d_raises(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(np.zeros(64, dtype=np.float32))
        with pytest.raises((ValueError, RuntimeError)):
            net.recall(np.zeros((1, 64), dtype=np.float32))

    def test_recall_negative_max_steps(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises(ValueError, match="max_steps"):
            net.recall(np.zeros(64, dtype=np.float32), max_steps=-1)


class TestEnergy:
    def test_energy_decreases_on_recall(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        pat = random_pattern(64, rng)
        net.store_pattern(pat)
        noisy = pat + rng.standard_normal(64).astype(np.float32) * 0.5
        e_before = net.energy(noisy)
        result = net.recall(noisy)
        e_after = net.energy(result.state)
        assert np.isfinite(e_before)
        assert np.isfinite(e_after)
        assert e_after <= e_before + 1e-5

    def test_energy_no_patterns(self):
        net = HopfieldNetwork(dim=6, seed=1)
        state = random_pattern(64, np.random.default_rng(42))
        with pytest.raises(ValueError, match="no patterns"):
            net.energy(state)

    def test_energy_2d_raises(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(np.zeros(64, dtype=np.float32))
        with pytest.raises((ValueError, RuntimeError)):
            net.energy(np.zeros((1, 64), dtype=np.float32))


class TestPatternManagement:
    def test_get_pattern_roundtrip(self):
        net = HopfieldNetwork(dim=6, seed=1)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        retrieved = net.get_pattern(0)
        np.testing.assert_allclose(retrieved, pat, atol=1e-7)

    def test_patterns_property(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        pats = [random_pattern(64, rng) for _ in range(3)]
        for p in pats:
            net.store_pattern(p)
        all_pats = net.patterns
        assert all_pats.shape == (3, 64)
        for i, p in enumerate(pats):
            np.testing.assert_allclose(all_pats[i], p, atol=1e-7)

    def test_patterns_empty(self):
        net = HopfieldNetwork(dim=6, seed=1)
        assert net.patterns.shape == (0, 64)

    def test_pop_pattern_removes_last(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        first = random_pattern(64, rng)
        second = random_pattern(64, rng)
        net.store_pattern(first)
        net.store_pattern(second)
        assert net.num_patterns == 2
        net.pop_pattern()
        assert net.num_patterns == 1
        np.testing.assert_allclose(net.get_pattern(0), first, atol=1e-7)

    def test_clear(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        for _ in range(5):
            net.store_pattern(random_pattern(64, rng))
        net.clear()
        assert net.num_patterns == 0

    def test_store_patterns_batch(self):
        net = HopfieldNetwork(dim=6, seed=1)
        rng = np.random.default_rng(42)
        batch = rng.standard_normal((4, 64)).astype(np.float32)
        net.store_patterns(batch)
        assert net.num_patterns == 4
        np.testing.assert_allclose(net.get_pattern(2), batch[2], atol=1e-7)

    def test_get_pattern_negative_index(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(np.zeros(64, dtype=np.float32))
        with pytest.raises(IndexError):
            net.get_pattern(-1)


class TestErrorHandling:
    def test_store_wrong_size(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises((ValueError, RuntimeError)):
            net.store_pattern(np.zeros(100, dtype=np.float32))

    def test_store_2d_raises(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises((ValueError, RuntimeError)):
            net.store_pattern(np.zeros((1, 64), dtype=np.float32))

    def test_recall_wrong_size(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(np.zeros(64, dtype=np.float32))
        with pytest.raises((ValueError, RuntimeError)):
            net.recall(np.zeros(100, dtype=np.float32))

    def test_energy_wrong_size(self):
        net = HopfieldNetwork(dim=6, seed=1)
        net.store_pattern(np.zeros(64, dtype=np.float32))
        with pytest.raises((ValueError, RuntimeError)):
            net.energy(np.zeros(100, dtype=np.float32))

    def test_get_pattern_out_of_range(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises((IndexError, RuntimeError)):
            net.get_pattern(0)

    def test_pop_empty(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises((IndexError, RuntimeError)):
            net.pop_pattern()

    def test_store_patterns_wrong_shape(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises(ValueError):
            net.store_patterns(np.zeros(64, dtype=np.float32))  # 1D, not 2D

    def test_store_patterns_wrong_width(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises(ValueError, match="num_vertices"):
            net.store_patterns(np.zeros((3, 100), dtype=np.float32))

    def test_store_patterns_3d_raises(self):
        net = HopfieldNetwork(dim=6, seed=1)
        with pytest.raises(ValueError):
            net.store_patterns(np.zeros((2, 3, 64), dtype=np.float32))


class TestPersistence:
    def test_pickle_roundtrip(self):
        net = HopfieldNetwork(dim=6, seed=42)
        rng = np.random.default_rng(42)
        for _ in range(3):
            net.store_pattern(random_pattern(64, rng))
        data = pickle.dumps(net)
        loaded = pickle.loads(data)
        assert loaded.dim == 6
        assert loaded.num_patterns == 3
        np.testing.assert_allclose(
            loaded.patterns, net.patterns, atol=1e-7,
        )

    def test_pickle_preserves_config(self):
        net = HopfieldNetwork(
            dim=8, seed=99, reach=2, beta=2.5,
            neighbor_fraction=0.5, tolerance=1e-4,
        )
        loaded = pickle.loads(pickle.dumps(net))
        assert loaded.seed == 99
        assert loaded.reach == 2
        assert loaded.beta == pytest.approx(2.5)
        assert loaded.neighbor_fraction == pytest.approx(0.5)
        assert loaded.tolerance == pytest.approx(1e-4)

    def test_pickle_recall_after_load(self):
        """Verify recall still works on a deserialized network."""
        net = HopfieldNetwork(dim=6, seed=42)
        rng = np.random.default_rng(42)
        pat = random_pattern(64, rng)
        net.store_pattern(pat)
        loaded = pickle.loads(pickle.dumps(net))
        result = loaded.recall(pat)
        assert result.converged
        assert overlap(result.state, pat) > 0.99

    def test_save_load(self, tmp_path):
        net = HopfieldNetwork(dim=6, seed=42)
        pat = random_pattern(64, np.random.default_rng(42))
        net.store_pattern(pat)
        path = tmp_path / "model.pkl"
        net.save(path)
        loaded = HopfieldNetwork.load(path)
        assert loaded.num_patterns == 1
        np.testing.assert_allclose(loaded.get_pattern(0), pat, atol=1e-7)

    def test_save_load_string_path(self, tmp_path):
        net = HopfieldNetwork(dim=6, seed=42)
        path = str(tmp_path / "model.pkl")
        net.save(path)
        loaded = HopfieldNetwork.load(path)
        assert loaded.dim == 6

    def test_pickle_empty(self):
        net = HopfieldNetwork(dim=6, seed=1)
        loaded = pickle.loads(pickle.dumps(net))
        assert loaded.num_patterns == 0
        assert loaded.dim == 6

    def test_pickle_future_version_raises(self):
        net = HopfieldNetwork(dim=6, seed=1)
        state = net.__getstate__()
        state["_version"] = 999
        net2 = object.__new__(HopfieldNetwork)
        with pytest.raises(ValueError, match="persistence version"):
            net2.__setstate__(state)

    def test_load_wrong_type(self, tmp_path):
        path = tmp_path / "bad.pkl"
        with open(path, "wb") as f:
            pickle.dump("not a network", f)
        with pytest.raises(TypeError, match="HopfieldNetwork"):
            HopfieldNetwork.load(path)

    def test_load_missing_file(self, tmp_path):
        with pytest.raises(FileNotFoundError):
            HopfieldNetwork.load(tmp_path / "nonexistent.pkl")


class TestConvenience:
    def test_dtype_coercion_float64(self):
        """float64 arrays are automatically converted to float32."""
        net = HopfieldNetwork(dim=6, seed=1)
        pat = np.random.default_rng(42).standard_normal(64)  # float64
        net.store_pattern(pat)
        assert net.num_patterns == 1

    def test_dtype_coercion_int(self):
        """Integer arrays are automatically converted to float32."""
        net = HopfieldNetwork(dim=6, seed=1)
        pat = np.ones(64, dtype=np.int32)
        net.store_pattern(pat)
        assert net.num_patterns == 1

    def test_version(self):
        assert hasattr(hh, "__version__")

    def test_large_dim_smoke(self):
        """Quick smoke test at dim=10 (N=1024) with store+recall."""
        net = HopfieldNetwork(dim=10, seed=42)
        rng = np.random.default_rng(42)
        pat = random_pattern(1024, rng)
        net.store_pattern(pat)
        result = net.recall(pat)
        assert result.converged
        assert overlap(result.state, pat) > 0.99
