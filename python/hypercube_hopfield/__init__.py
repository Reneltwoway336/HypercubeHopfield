"""HypercubeHopfield: modern Hopfield associative memory on hypercube graphs.

This package provides Python bindings for the HypercubeHopfield C++ library.
The network topology is a Boolean hypercube graph with N = 2^dim neurons.
Patterns are stored explicitly and retrieved via softmax attention over
sparse local neighborhoods.

Quick start::

    import numpy as np
    import hypercube_hopfield as hh

    net = hh.HopfieldNetwork(dim=8, seed=42)
    patterns = np.random.randn(10, net.num_vertices).astype(np.float32)
    net.store_patterns(patterns)

    cue = patterns[0] + np.random.randn(net.num_vertices).astype(np.float32) * 0.3
    result = net.recall(cue)
    print(f"Converged: {result.converged}, steps: {result.steps}")
"""

from __future__ import annotations

import pathlib
import pickle
from typing import NamedTuple

import numpy as np

from ._core import UpdateMode, _HopfieldNetwork

__version__ = "0.1.0"
__all__ = ["HopfieldNetwork", "RecallResult", "UpdateMode", "__version__"]


def _to_float32(arr):
    """Ensure array is C-contiguous float32."""
    return np.ascontiguousarray(arr, dtype=np.float32)


class RecallResult(NamedTuple):
    """Result of a recall operation.

    Attributes
    ----------
    state : ndarray
        The recalled (cleaned) state, shape ``(N,)``.
    steps : int
        Number of update sweeps performed.
    converged : bool
        True if the state stabilized within ``tolerance``.
    """

    state: np.ndarray
    steps: int
    converged: bool


class HopfieldNetwork:
    """Modern Hopfield associative memory on a Boolean hypercube graph.

    The network has N = 2^dim neurons arranged on the vertices of a
    dim-dimensional Boolean hypercube. Patterns are stored explicitly and
    recalled via softmax attention over sparse Hamming-ball neighborhoods.

    .. note::

        This class is **not thread-safe**. Do not share instances across
        threads. Create separate instances for concurrent use. The GIL is
        released during ``recall()`` to allow other Python threads to run,
        but the same network must not be accessed concurrently.

    Parameters
    ----------
    dim : int
        Hypercube dimension (4-16). Determines neuron count: N = 2^dim.
    seed : int
        RNG seed for update order permutations. Default: 0.
    reach : int
        Hamming-ball radius for neighbor connectivity (1 to dim).
        Default: 0, which selects dim // 2 (~50-63% connectivity).
    beta : float
        Inverse temperature for softmax attention. Higher = sharper recall
        (closer to winner-take-all). Default: 4.0.
    neighbor_fraction : float
        Fraction of the Hamming ball to use, in (0.0, 1.0]. Masks sorted
        by distance (closest first), then truncated. Default: 1.0.
    tolerance : float
        Convergence threshold for recall. A sweep is stable when no vertex
        changes by more than this value. Default: 1e-6.

    Examples
    --------
    >>> import numpy as np
    >>> import hypercube_hopfield as hh
    >>> net = hh.HopfieldNetwork(dim=8, seed=42)
    >>> pat = np.random.randn(256).astype(np.float32)
    >>> net.store_pattern(pat)
    >>> result = net.recall(pat + np.random.randn(256).astype(np.float32) * 0.1)
    >>> result.converged
    True
    """

    def __init__(
        self,
        dim: int,
        *,
        seed: int = 0,
        reach: int = 0,
        beta: float = 4.0,
        neighbor_fraction: float = 1.0,
        tolerance: float = 1e-6,
    ):
        if not isinstance(dim, int) or not (4 <= dim <= 16):
            raise ValueError(f"dim must be an integer in [4, 16], got {dim}")
        if not isinstance(seed, int) or seed < 0:
            raise ValueError(f"seed must be a non-negative integer, got {seed}")
        if not isinstance(reach, int) or reach < 0:
            raise ValueError(f"reach must be a non-negative integer, got {reach}")
        if beta <= 0:
            raise ValueError(f"beta must be positive, got {beta}")
        if not (0.0 < neighbor_fraction <= 1.0):
            raise ValueError(
                f"neighbor_fraction must be in (0.0, 1.0], got {neighbor_fraction}"
            )
        if tolerance <= 0:
            raise ValueError(f"tolerance must be positive, got {tolerance}")
        self._impl = _HopfieldNetwork(
            dim, seed, reach, beta, neighbor_fraction, tolerance,
        )

    # ── Pattern storage ──

    def store_pattern(self, pattern: np.ndarray) -> None:
        """Store a single pattern.

        Parameters
        ----------
        pattern : ndarray
            1D array of N = 2^dim continuous-valued floats.
            Converted to float32 automatically.
        """
        self._impl.store_pattern(_to_float32(pattern))

    def store_patterns(self, patterns: np.ndarray) -> None:
        """Store multiple patterns from a 2D array.

        Parameters
        ----------
        patterns : ndarray
            2D array of shape ``(num_patterns, num_vertices)``.
            Each row is stored as a separate pattern.
        """
        patterns = _to_float32(patterns)
        if patterns.ndim != 2:
            raise ValueError(
                f"patterns must be 2D, got {patterns.ndim}D"
            )
        if patterns.shape[1] != self.num_vertices:
            raise ValueError(
                f"patterns.shape[1] ({patterns.shape[1]}) != "
                f"num_vertices ({self.num_vertices})"
            )
        for i in range(patterns.shape[0]):
            self._impl.store_pattern(patterns[i])

    # ── Recall ──

    def recall(
        self,
        cue: np.ndarray,
        *,
        max_steps: int = 100,
        mode: UpdateMode = UpdateMode.Sync,
    ) -> RecallResult:
        """Recall a stored pattern from a (possibly noisy) cue.

        The input cue is **not** modified. A clean copy is returned.

        Parameters
        ----------
        cue : ndarray
            1D array of N floats. Typically a corrupted version of a
            stored pattern.
        max_steps : int
            Maximum update sweeps. Default: 100.
        mode : UpdateMode
            ``UpdateMode.Sync`` (default, deterministic, GPU-portable) or
            ``UpdateMode.Async`` (guaranteed energy descent).

        Returns
        -------
        RecallResult
            Named tuple with fields ``state``, ``steps``, ``converged``.

        Examples
        --------
        >>> result = net.recall(noisy_cue)
        >>> print(f"Converged in {result.steps} steps: {result.converged}")
        >>> recalled_state = result.state
        """
        if not isinstance(max_steps, int) or max_steps < 0:
            raise ValueError(
                f"max_steps must be a non-negative integer, got {max_steps}"
            )
        state, steps, converged = self._impl.recall(
            _to_float32(cue), max_steps, mode,
        )
        return RecallResult(state, steps, converged)

    # ── Energy ──

    def energy(self, state: np.ndarray) -> float:
        """Compute the modern Hopfield energy for a state.

        Parameters
        ----------
        state : ndarray
            1D array of N floats.

        Returns
        -------
        float
            Energy value. Lower = closer to a stored pattern.

        Raises
        ------
        ValueError
            If no patterns are stored.
        """
        return self._impl.energy(_to_float32(state))

    # ── Pattern management ──

    def get_pattern(self, idx: int) -> np.ndarray:
        """Read back a stored pattern by index.

        Parameters
        ----------
        idx : int
            Pattern index in [0, num_patterns).

        Returns
        -------
        ndarray
            1D array of N floats (copy).

        Raises
        ------
        IndexError
            If ``idx`` is out of range.
        """
        if not isinstance(idx, int) or idx < 0:
            raise IndexError(
                f"idx must be a non-negative integer, got {idx}"
            )
        return self._impl.get_pattern(idx)

    @property
    def patterns(self) -> np.ndarray:
        """All stored patterns as a 2D array.

        Returns a **copy** of all stored patterns. For networks with many
        patterns at high dim, this allocates O(num_patterns * 2^dim) memory.

        Returns
        -------
        ndarray
            Shape ``(num_patterns, num_vertices)``. Empty ``(0, N)``
            array if no patterns are stored.
        """
        m = self.num_patterns
        n = self.num_vertices
        if m == 0:
            return np.empty((0, n), dtype=np.float32)
        return np.stack(
            [self._impl.get_pattern(i) for i in range(m)]
        )

    def pop_pattern(self) -> None:
        """Remove the most recently stored pattern.

        Raises
        ------
        IndexError
            If no patterns are stored.
        """
        self._impl.pop_pattern()

    def clear(self) -> None:
        """Remove all stored patterns and reset internal state."""
        self._impl.clear()

    # ── Properties ──

    @property
    def dim(self) -> int:
        """Hypercube dimension."""
        return self._impl.dim

    @property
    def num_vertices(self) -> int:
        """Number of neurons (2^dim)."""
        return self._impl.num_vertices

    @property
    def num_patterns(self) -> int:
        """Number of stored patterns."""
        return self._impl.num_patterns

    @property
    def seed(self) -> int:
        """RNG seed used at construction."""
        return self._impl.seed

    @property
    def reach(self) -> int:
        """Hamming-ball radius for neighbor connectivity."""
        return self._impl.reach

    @property
    def neighbor_fraction(self) -> float:
        """Fraction of the Hamming ball used."""
        return self._impl.neighbor_fraction

    @property
    def beta(self) -> float:
        """Inverse temperature for softmax attention."""
        return self._impl.beta

    @property
    def tolerance(self) -> float:
        """Convergence threshold for recall."""
        return self._impl.tolerance

    def __repr__(self) -> str:
        return (
            f"HopfieldNetwork(dim={self.dim}, N={self.num_vertices}, "
            f"patterns={self.num_patterns}, reach={self.reach}, "
            f"beta={self.beta})"
        )

    # ── Persistence ──

    _PERSISTENCE_VERSION = 1

    def __getstate__(self) -> dict:
        """Serialize network state for pickling.

        Persists the constructor config and all stored patterns.
        """
        return {
            "_version": self._PERSISTENCE_VERSION,
            "dim": self.dim,
            "seed": self.seed,
            "reach": self.reach,
            "beta": self.beta,
            "neighbor_fraction": self.neighbor_fraction,
            "tolerance": self.tolerance,
            "patterns": self.patterns,
        }

    def __setstate__(self, state: dict) -> None:
        """Restore network from pickled state.

        Called by pickle on an object created via ``object.__new__``,
        so ``self._impl`` does not exist yet. ``__init__`` is safe to
        call here as it unconditionally assigns ``self._impl``.
        """
        version = state.get("_version", 0)
        if version > self._PERSISTENCE_VERSION:
            raise ValueError(
                f"Model was saved with persistence version {version}, "
                f"but this version only supports up to "
                f"{self._PERSISTENCE_VERSION}. Upgrade hypercube-hopfield."
            )
        self.__init__(
            dim=state["dim"],
            seed=state["seed"],
            reach=state["reach"],
            beta=state["beta"],
            neighbor_fraction=state["neighbor_fraction"],
            tolerance=state["tolerance"],
        )
        patterns = state["patterns"]
        if len(patterns) > 0:
            self.store_patterns(patterns)

    def save(self, path: str | pathlib.Path) -> None:
        """Save the network to a file.

        Saves the configuration and all stored patterns. The file is a
        standard Python pickle.

        Parameters
        ----------
        path : str or Path
            File path to write.

        Examples
        --------
        >>> net.save("model.pkl")
        >>> loaded = hh.HopfieldNetwork.load("model.pkl")
        """
        with open(pathlib.Path(path), "wb") as f:
            pickle.dump(self, f, protocol=pickle.HIGHEST_PROTOCOL)

    @classmethod
    def load(cls, path: str | pathlib.Path) -> "HopfieldNetwork":
        """Load a saved network from a file.

        .. warning::

            This method uses ``pickle.load()`` internally. Never load
            files from untrusted sources -- a malicious pickle file can
            execute arbitrary code.

        Parameters
        ----------
        path : str or Path
            File path to read.

        Returns
        -------
        HopfieldNetwork
            The restored network with all stored patterns intact.
        """
        with open(pathlib.Path(path), "rb") as f:
            obj = pickle.load(f)
        if not isinstance(obj, cls):
            raise TypeError(f"Expected HopfieldNetwork, got {type(obj).__name__}")
        return obj
