# Copyright (c) 2022 CNES
#
# All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.
import copy
import os
import pickle
import pytest
import netCDF4
try:
    import matplotlib.pyplot
    import matplotlib.colors
    HAVE_PLT = True
except ImportError:
    HAVE_PLT = False
import numpy as np
from ... import core
from .. import grid2d_path, make_or_compare_reference


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


def load_data():
    with netCDF4.Dataset(grid2d_path()) as ds:  # type: ignore
        z = ds.variables['mss'][:].T
        z[z.mask] = float("nan")
        return ds.variables['lon'][:], ds.variables['lat'][:], z.data


def test_binning2d_acessors():
    x_axis = core.Axis(np.linspace(-180, 180, 10), is_circle=True)
    y_axis = core.Axis(np.linspace(-90, 90, 10))

    binning = core.Binning2DFloat64(x_axis, y_axis)
    assert isinstance(binning.x, core.Axis)
    assert isinstance(binning.y, core.Axis)

    # The class must return a reference on the axes provided during
    # construction
    assert id(x_axis) == id(binning.x)
    assert id(y_axis) == id(binning.y)

    binning.clear()
    count = binning.count()
    assert isinstance(count, np.ndarray)
    assert count.size == len(x_axis) * len(y_axis)
    assert count.mean() == 0


def test_binning2d_methods(pytestconfig):
    visualize = pytestconfig.getoption('visualize')
    dump = pytestconfig.getoption('dump')
    x_axis = core.Axis(np.linspace(-180, 180, 361 // 4), is_circle=True)
    y_axis = core.Axis(np.linspace(-90, 90, 180 // 4))

    binning = core.Binning2DFloat64(x_axis, y_axis, None)
    x, y, z = load_data()
    mx, my = np.meshgrid(x, y, indexing='ij')
    binning.push(mx.ravel(), my.ravel(), z.ravel())

    count = binning.count()
    assert count.max() != 0
    simple_mean = np.ma.fix_invalid(binning.mean())
    if HAVE_PLT and visualize:
        mx, my = np.meshgrid(x_axis[:], y_axis[:], indexing='ij')
        plot(mx, my, simple_mean, "binning2d_simple.png")

    mx, my = np.meshgrid(x, y, indexing='ij')
    binning.clear()
    binning.push(mx.ravel(), my.ravel(), z.ravel(), simple=False)
    make_or_compare_reference("binning2d_simple.npy", binning.mean(), dump)

    count = binning.count()
    assert count.max() != 0
    linear_mean = np.ma.fix_invalid(binning.mean())
    if HAVE_PLT and visualize:
        mx, my = np.meshgrid(x_axis[:], y_axis[:], indexing='ij')
        plot(mx, my, linear_mean, "binning2d_linear.png")

    assert not np.all(linear_mean == simple_mean)

    # Test of access to statistical variables
    make_or_compare_reference("binning2d_linear.npy", binning.mean(), dump)
    assert isinstance(binning.kurtosis(), np.ndarray)
    assert isinstance(binning.max(), np.ndarray)
    assert isinstance(binning.min(), np.ndarray)
    assert isinstance(binning.skewness(), np.ndarray)
    assert isinstance(binning.sum(), np.ndarray)
    assert isinstance(binning.sum_of_weights(), np.ndarray)
    assert isinstance(binning.variance(), np.ndarray)


def test_binning2d_pickle():
    x_axis = core.Axis(np.linspace(-180, 180, 1), is_circle=True)
    y_axis = core.Axis(np.linspace(-90, 90, 1))

    binning = core.Binning2DFloat64(x_axis, y_axis, None)
    binning.push(np.array([-180]), np.array([-90]), np.array([np.pi]))

    assert np.all(binning.count() == 1)
    assert np.all(binning.mean() == np.pi)
    assert np.all(binning.min() == np.pi)
    assert np.all(binning.max() == np.pi)
    assert np.all(binning.sum() == np.pi)
    assert np.all(binning.sum_of_weights() == 1)
    assert np.all(binning.variance() == 0)
    assert np.all(np.isnan(binning.skewness()))
    assert np.all(np.isnan(binning.kurtosis()))

    other = pickle.loads(pickle.dumps(binning))

    assert np.all(other.count() == 1)
    assert np.all(other.mean() == np.pi)
    assert np.all(other.min() == np.pi)
    assert np.all(other.max() == np.pi)
    assert np.all(other.sum() == np.pi)
    assert np.all(other.sum_of_weights() == 1)
    assert np.all(other.variance() == 0)
    assert np.all(np.isnan(other.skewness()))
    assert np.all(np.isnan(other.kurtosis()))


def test_binning2d_iadd():
    x_axis = core.Axis(np.linspace(-180, 180, 1), is_circle=True)
    y_axis = core.Axis(np.linspace(-90, 90, 1))

    binning = core.Binning2DFloat64(x_axis, y_axis, None)
    binning.push(np.array([-180]), np.array([-90]), np.array([np.pi]))

    other = copy.copy(binning)
    other += binning

    assert np.all(binning.count() == 1)
    assert np.all(binning.mean() == np.pi)
    assert np.all(binning.min() == np.pi)
    assert np.all(binning.max() == np.pi)
    assert np.all(binning.sum() == np.pi)
    assert np.all(binning.sum_of_weights() == 1)
    assert np.all(binning.variance() == 0)
    assert np.all(np.isnan(other.skewness()))
    assert np.all(np.isnan(other.kurtosis()))

    assert np.all(other.count() == 2)
    assert np.all(other.mean() == np.pi)
    assert np.all(other.min() == np.pi)
    assert np.all(other.max() == np.pi)
    assert np.all(other.sum() == np.pi * 2)
    assert np.all(other.sum_of_weights() == 2)
    assert np.all(other.variance() == 0)
    assert np.all(np.isnan(other.skewness()))
    assert np.all(np.isnan(other.kurtosis()))
