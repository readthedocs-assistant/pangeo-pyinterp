# Copyright (c) 2022 CNES
#
# All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.
import os
import pickle
import netCDF4
try:
    import matplotlib.pyplot
    import matplotlib.colors
    HAVE_PLT = True
except ImportError:
    HAVE_PLT = False
import numpy as np
from ... import core
from .. import grid2d_path


def plot(x, y, z, filename):
    figure = matplotlib.pyplot.figure(figsize=(15, 15), dpi=150)
    value = z.mean()
    std = z.std()
    normalize = matplotlib.colors.Normalize(vmin=value - 3 * std,
                                            vmax=value + 3 * std)
    axe = figure.add_subplot(2, 1, 1)
    axe.pcolormesh(x, y, z, cmap='jet', norm=normalize, shading='auto')
    figure.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                filename),
                   bbox_inches='tight',
                   pad_inches=0.4)


def load_data(packing=True):
    """Creating the search tree"""
    with netCDF4.Dataset(grid2d_path()) as ds:  # type: ignore
        z = ds.variables['mss'][:].T
        z[z.mask] = float("nan")
        x = ds.variables['lon'][:]
        y = ds.variables['lat'][:]
        # Since insertion is slower, the data are sub-sampled to avoid
        # the test being too long.
        if not packing:
            x = x[::5]
            y = y[::5]
            z = z[::5, ::5]
        x = x.astype("float32")
        y = y.astype("float32")
        z = z.astype("float32")
        x, y = np.meshgrid(x, y, indexing='ij')
        mesh = core.RTree3DFloat32(core.geodetic.System())
        if packing:
            mesh.packing(np.vstack((x.ravel(), y.ravel())).T, z.data.ravel())
        else:
            mesh.insert(np.vstack((x.ravel(), y.ravel())).T, z.data.ravel())
        return mesh


def test_rtree_idw(pytestconfig):
    """Interpolation test"""
    mesh = load_data()
    lon = np.arange(-180, 180, 1 / 3.0, dtype="float32") + 1 / 3.0
    lat = np.arange(-90, 90, 1 / 3.0, dtype="float32") + 1 / 3.0
    x, y = np.meshgrid(lon, lat, indexing="ij")
    z0, _ = mesh.inverse_distance_weighting(np.vstack(
        (x.ravel(), y.ravel())).T,
                                            within=False,
                                            radius=None,
                                            k=8,
                                            num_threads=0)
    z1, _ = mesh.inverse_distance_weighting(np.vstack(
        (x.ravel(), y.ravel())).T,
                                            within=False,
                                            radius=None,
                                            k=8,
                                            num_threads=1)
    z0 = np.ma.fix_invalid(z0)
    z1 = np.ma.fix_invalid(z1)
    assert np.all(z1 == z0)

    if HAVE_PLT and pytestconfig.getoption("visualize"):
        plot(x, y, z0.reshape((len(lon), len(lat))), "mss_rtree_idw.png")


def test_rtree_rbf(pytestconfig):
    """Interpolation test"""
    mesh = load_data()
    lon = np.arange(-180, 180, 1 / 3.0, dtype="float32") + 1 / 3.0
    lat = np.arange(-90, 90, 1 / 3.0, dtype="float32") + 1 / 3.0
    x, y = np.meshgrid(lon, lat, indexing="ij")
    z0, _ = mesh.radial_basis_function(
        np.vstack((x.ravel(), y.ravel())).T,
        within=False,
        radius=None,
        rbf=core.RadialBasisFunction.InverseMultiquadric,
        epsilon=75000,  # 75 Km
        smooth=0,
        k=11,
        num_threads=1)
    z1, _ = mesh.radial_basis_function(
        np.vstack((x.ravel(), y.ravel())).T,
        within=False,
        radius=None,
        rbf=core.RadialBasisFunction.InverseMultiquadric,
        epsilon=75000,  # 75 Km
        smooth=0,
        k=11,
        num_threads=1)
    z0 = np.ma.fix_invalid(z0)
    z1 = np.ma.fix_invalid(z1)
    assert np.all(z1 == z0)

    if HAVE_PLT and pytestconfig.getoption("visualize"):
        plot(x, y, z0.reshape((len(lon), len(lat))), "mss_rtree_rbf.png")


def test_rtree_window_function(pytestconfig):
    """Interpolation test"""
    mesh = load_data()
    lon = np.arange(-180, 180, 1 / 3.0, dtype="float32") + 1 / 3.0
    lat = np.arange(-90, 90, 1 / 3.0, dtype="float32") + 1 / 3.0
    x, y = np.meshgrid(lon, lat, indexing="ij")
    z0, _ = mesh.window_function(np.vstack((x.ravel(), y.ravel())).T,
                                 within=False,
                                 radius=2_000_000,
                                 wf=core.WindowFunction.Hamming,
                                 k=11,
                                 num_threads=1)
    z1, _ = mesh.window_function(np.vstack((x.ravel(), y.ravel())).T,
                                 within=False,
                                 radius=2_000_000,
                                 wf=core.WindowFunction.Hamming,
                                 k=11,
                                 num_threads=1)
    z0 = np.ma.fix_invalid(z0)
    z1 = np.ma.fix_invalid(z1)
    assert np.all(z1 == z0)

    if HAVE_PLT and pytestconfig.getoption("visualize"):
        plot(x, y, z0.reshape((len(lon), len(lat))), "mss_rtree_wf.png")


def test_rtree_insert():
    """Data insertion test"""
    mesh = load_data(packing=False)
    assert isinstance(mesh, core.RTree3DFloat32)
    assert len(mesh) != 0


def test_rtree_query():
    """Data insertion test"""
    mesh = load_data(packing=True)
    assert len(mesh) != 0
    distances, values = mesh.query(np.vstack((np.array([0]), np.array([0]))).T)
    assert distances.shape == (1, 4)
    assert values.shape == (1, 4)
    assert distances[0, 0] == 0


def test_rtree_pickle():
    """Serialization test"""
    interpolator = load_data()
    other = pickle.loads(pickle.dumps(interpolator))
    assert isinstance(other, core.RTree3DFloat32)
