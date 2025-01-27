# Copyright (c) 2022 CNES
#
# All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE file.
from typing import Any, Iterable, Optional, Union
import dask.array as da
import numpy as np
from .. import core


def _delayed(
    attr: str,
    values: da.Array,
    weights: Optional[da.Array] = None,
    axis: Optional[Iterable[int]] = None,
) -> Union[core.DescriptiveStatisticsFloat64,
           core.DescriptiveStatisticsFloat32]:
    """Calculate the descriptive statistics of a dask array."""
    if weights is not None and values.shape != weights.shape:
        raise ValueError("values and weights must have the same shape")

    def _process_block(attr, x, w, axis):
        instance = getattr(core, attr)(values=x, weights=w, axis=axis)
        return np.array([instance], dtype="object")

    drop_axis = list(range(values.ndim))[1:]

    return da.map_blocks(_process_block,
                         attr,
                         values,
                         weights,
                         axis,
                         drop_axis=drop_axis,
                         dtype="object").sum().compute()  # type: ignore


class DescriptiveStatistics:
    """
    Univariate descriptive statistics.

    Calculates the incremental descriptive statistics from the provided values.
    The calculation of the statistics is done when the constructor is invoked.
    Different methods allow to extract the calculated statistics.

    .. seealso::

        Pébay, P., Terriberry, T.B., Kolla, H. et al.
        Numerically stable, scalable formulas for parallel and online
        computation of higher-order multivariate central moments
        with arbitrary weights.
        Comput Stat 31, 1305–1325,
        2016,
        https://doi.org/10.1007/s00180-015-0637-z
    """
    def __init__(self,
                 values: Union[da.Array, np.ndarray],
                 weights: Optional[Union[da.Array, np.ndarray]] = None,
                 axis: Optional[Union[int, Iterable[int]]] = None,
                 dtype: Optional[np.dtype] = None) -> None:
        """Creates a new descriptive statistics container.

        Args:
            values (numpy.ndarray, dask.Array): Array containing numbers whose
                statistics are desired.

                .. note::

                    NaNs are automatically ignored.

            weights (numpy.ndarray, dask.Array, optional): An array of weights
                associated with the values. If not provided, all values are
                assumed to have equal weight.
            axis (int, iterable, optional): Axis or axes along which to compute
                the statistics. If not provided, the statistics are computed
                over the flattened array.    
            dtype (numpy.dtype, optional): Data type of the returned array. By
                default, the data type is numpy.float64.
        """
        if isinstance(axis, int):
            axis = (axis, )
        dtype = dtype or np.dtype("float64")
        if dtype == np.dtype("float64"):
            attr = f"DescriptiveStatisticsFloat64"
        elif dtype == np.dtype("float32"):
            attr = f"DescriptiveStatisticsFloat32"
        else:
            raise ValueError(f"dtype {dtype} not handled by the object")
        if isinstance(values, da.Array) or isinstance(weights, da.Array):
            self._instance = _delayed(
                attr, da.asarray(values),
                da.asarray(weights) if weights is not None else None, axis)
        else:
            self._instance: Union[core.DescriptiveStatisticsFloat64,
                                  core.DescriptiveStatisticsFloat32] = getattr(
                                      core, attr)(values, weights, axis)

    def __iadd__(self, other: Any) -> "DescriptiveStatistics":
        """Adds a new descriptive statistics container to the current one.

        Returns:
            DescriptiveStatistics: Returns itself.
        """
        if isinstance(other, DescriptiveStatistics):
            if type(self._instance) != type(other._instance):
                raise TypeError(
                    "Descriptive statistics must have the same type")
            self._instance += other._instance  # type: ignore
        else:
            raise TypeError("unsupported operand type(s) for +="
                            f": '{type(self)}' and '{type(other)}'")
        return self

    def count(self) -> np.ndarray:
        """Returns the count of samples.
        
        Returns:
            numpy.ndarray: Returns the count of samples.
        """
        return self._instance.count()

    def kurtosis(self) -> np.ndarray:
        """Returns the kurtosis of samples.
        
        Returns:
            numpy.ndarray: Returns the kurtosis of samples.
        """
        return self._instance.kurtosis()

    def max(self) -> np.ndarray:
        """Returns the maximum of samples.
        
        Returns:
            numpy.ndarray: Returns the maximum of samples.
        """
        return self._instance.max()

    def mean(self) -> np.ndarray:
        """Returns the mean of samples.
        
        Returns:
            numpy.ndarray: Returns the mean of samples.
        """
        return self._instance.mean()

    def min(self) -> np.ndarray:
        """Returns the minimum of samples.
        
        Returns:
            numpy.ndarray: Returns the minimum of samples.
        """
        return self._instance.min()

    def skewness(self) -> np.ndarray:
        """Returns the skewness of samples.
        
        Returns:
            numpy.ndarray: Returns the skewness of samples.
        """
        return self._instance.skewness()

    def sum(self) -> np.ndarray:
        """Returns the sum of samples.
        
        Returns:
            numpy.ndarray: Returns the sum of samples.
        """
        return self._instance.sum()

    def sum_of_weights(self) -> np.ndarray:
        """Returns the sum of weights.
        
        Returns:
            numpy.ndarray: Returns the sum of weights.
        """
        return self._instance.sum_of_weights()

    def var(self, ddof: int = 0) -> np.ndarray:
        """Returns the variance of samples.
        
        Args:
            ddof (int, optional): Means Delta Degrees of Freedom. The divisor
                used in calculations is N - ddof, where N represents the number
                of elements. By default ddof is zero.

        Returns:
            numpy.ndarray: Returns the variance of samples.
        """
        return self._instance.variance(ddof)

    def std(self, ddof: int = 0) -> np.ndarray:
        """Returns the standard deviation of samples.

        Args:
            ddof (int, optional): Means Delta Degrees of Freedom. The divisor
                used in calculations is N - ddof, where N represents the number
                of elements. By default ddof is zero.

        Returns:
            numpy.ndarray: Returns the standard deviation of samples.
        """
        return np.sqrt(self.var(ddof=ddof))

    def array(self) -> np.ndarray:
        """Returns the different statistical variables calculated in a numpy
        structured table with the following fields:

        - count: Number of samples.
        - kurtosis: Kurtosis of samples.
        - max: Maximum of samples.
        - mean: Mean of samples.
        - min: Minimum of samples.
        - skewness: Skewness of samples.
        - sum_of_weights: Sum of weights.
        - sum: Sum of samples.
        - var: Variance of samples (ddof is equal to zero).

        Returns:
            numpy.ndarray: Returns the different statistical variables
                calculated in a numpy structured table.
        """
        dreal = 'f8' if isinstance(self._instance,
                                   core.DescriptiveStatisticsFloat64) else 'f4'
        dtype = [('count', 'u8'), ('kurtosis', dreal), ('max', dreal),
                 ('mean', dreal), ('min', dreal), ('skewness', dreal),
                 ('sum_of_weights', dreal), ('sum', dreal), ('var', dreal)]
        fields = [item[0] for item in dtype]
        field = fields.pop()
        buffer = getattr(self, field)()
        result = np.empty(buffer.shape, dtype=dtype)
        result[field] = buffer
        for field in fields:
            result[field] = getattr(self, field)()
        return result

    def __str__(self) -> str:
        array, shape = self._instance.__getstate__()
        return str(array.reshape(shape))
