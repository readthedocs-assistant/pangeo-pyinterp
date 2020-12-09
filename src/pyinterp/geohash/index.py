"""
Geogrophic Index
----------------
"""
from typing import Any, Dict, Iterable, List, Optional, Tuple
import json
import numpy
import xarray
from . import lock
from . import storage
from .. import core
from .. import geodetic
from ..core.geohash import string


class GeoHash:
    """
    Geogrophic index based on GeoHash encoding.

    Args:
        store (AbstractMutableMapping): Object managing the storage of the
            index
        precision (int): Accuracy of the index. By default the precision is 3
            characters. The table below gives the correspondence between the
            number of characters (i.e. the ``precision`` parameter of this
            constructor), the size of the boxes of the grid at the equator and
            the total number of boxes.

            =========  ===============  ==========
            precision  lng/lat (km)     samples
            =========  ===============  ==========
            1          4950/4950        32
            2          618.75/1237.50   1024
            3          154.69/154.69    32768
            4          19.34/38.67      1048576
            5          4.83/4.83        33554432
            6          0.60/1.21        1073741824
            =========  ===============  ==========
        synchronizer (lock.Synchronizer, optional): Write synchronizer
    """
    PROPERTIES = b'.properties'

    def __init__(self,
                 store: storage.AbstractMutableMapping,
                 precision: int = 3,
                 synchronizer: Optional[lock.Synchronizer] = None) -> None:
        self._store = store
        self._precision = precision
        self._synchronizer = synchronizer or lock.PuppetSynchronizer()

    @property
    def store(self) -> storage.AbstractMutableMapping:
        """Gets the object hndling the storage of this instance"""
        return self._store

    @property
    def precision(self) -> int:
        """Accuracy of this instance"""
        return self._precision

    def set_properties(self) -> None:
        """Definition of index properties"""
        if self.PROPERTIES in self._store:
            raise RuntimeError("index already initialized")
        self._store[self.PROPERTIES] = json.dumps(
            {'precision': self._precision})

    @classmethod
    def get_properties(cls, store) -> Dict[str, Any]:
        """Reading index properties

        Return:
            dict: Index properties (number of character used to encode a
            position)
        """
        precision = store[cls.PROPERTIES]
        if isinstance(precision, list):
            precision = precision[0]
        return json.loads(precision)

    def encode(self,
               lon: numpy.ndarray,
               lat: numpy.ndarray,
               normalize: bool = True,
               unicode: bool = False) -> numpy.ndarray:
        """Encode points into geohash with the given precision

        Args:
            lon (numpy.ndarray): Longitudes in degrees of the positions to be
                encoded.
            lat (numpy.ndarray): Latitudes in degrees of the positions to be
                encoded.
            normalize (bool): If true, normalize longitude between [-180, 180[
            unicode (bool): If true, transforms GeoHash codes into unicode
                strings.

        Return:
            numpy.ndarray: geohash code for each coordinates of the points
            read from the vectors provided.
        """
        if normalize:
            lon = (lon + 180) % 360 - 180
        result = string.encode(lon, lat, precision=self._precision)
        if unicode:
            return result.astype('U')
        return result

    def update(self, other: Iterable[Tuple[bytes, Any]]) -> None:
        """Update the index with the key/value pairs from data, overwriting
        existing keys.

        Args:
            other (iterable): Geohash codes associated with the values to be
                stored in the database.
        """
        with self._synchronizer:
            self._store.update(other)

    def extend(self, other: Iterable[Tuple[bytes, Any]]) -> None:
        """Update the index with the key/value pairs from data, appending
        existing keys with the new data.

        Args:
            other (iterable): Geohash codes associated with the values to be
                updated in the database.
        """
        with self._synchronizer:
            self._store.extend(other)

    def keys(self, box: Optional[geodetic.Box] = None) -> Iterable[bytes]:
        """Returns all hash defined in the index
        
        Args:
            box (pyinterp.geodetic.Box, optional): If true, the method returns
                the codes defined in the supplied area, otherwise all the codes
                stored in the index.
        
        Return:
            iterable: keys selected in the index.
        """
        result = filter(lambda item: item != self.PROPERTIES,
                        self._store.keys())
        if box is None:
            return result
        return set(string.bounding_boxes(
            box, precision=self._precision)).intersection(set(result))

    def box(self, box: Optional[geodetic.Box] = None) -> List[Any]:
        """Selection of all data within the defined geographical area

        Args:
            box (pyinterp.geodetic.Box): Bounding box used for data selection.

        Return:
            list: List of data contained in the database for all positions
            located in the selected geographic region.
        """
        return list(
            filter(
                lambda item: len(item) != 0,
                self._store.values(
                    list(string.bounding_boxes(box,
                                               precision=self._precision)))))

    def values(self, keys: Optional[Iterable[bytes]] = None) -> List[Any]:
        """Returns the list of values defined in the index

        Args:
            keys (iterable, optional): The list of keys to be selected. If
                this parameter is undefined, the method returns all values
                defined in the index.
        
        Return:
            list: values selected in the index.
        """
        keys = keys or self.keys()
        return self._store.values(list(keys))

    def items(
            self,
            keys: Optional[Iterable[bytes]] = None) -> List[Tuple[bytes, Any]]:
        """Returns the list of pair (key, value) defined in the index

        Args:
            keys (iterable, optional): The list of keys to be selected. If
                this parameter is undefined, the method returns all items
                defined in the index.
        
        Return:
            list: items selected in the index.
        """
        keys = keys or self.keys()
        return self._store.items(list(keys))

    def to_xarray(self,
                  box: Optional[geodetic.Box] = None) -> xarray.DataArray:
        """Get the XArray containing the data selected in the index.

        Args:
            box (pyinterp.geodetic.Box): Bounding box used for data selection.
        
        Return:
            list: items selected in the index.
        """
        keys = list(self.keys(box))
        if len(keys) == 0:
            hashs = numpy.array([])
            data = numpy.hstack([])
        else:
            hashs = numpy.array(keys)
            data = numpy.array(self.values(keys), dtype=object)

        return to_xarray(hashs, data.squeeze())

    @staticmethod
    def where(
        hash_codes: numpy.ndarray
    ) -> Dict[int, Tuple[Tuple[int, int], Tuple[int, int]]]:
        """Returns the start and end indexes of the different GeoHash boxes.

        Args:
            hash_codes (numpy.ndarray): geohash codes obtained by the `encode`
            method

        Return:
            dict: the start and end indexes for each geohash boxes
        """
        return string.where(hash_codes)

    def __len__(self):
        return len(self._store) - 1

    def __repr__(self) -> str:
        return f"<{self.__class__.__name__} precision={self._precision}>"


def init_geohash(store: storage.AbstractMutableMapping,
                 precision: int = 3,
                 synchronizer: Optional[lock.Synchronizer] = None) -> GeoHash:
    """Creation of a GeoHash index

    Args:
        store (AbstractMutableMapping): Object managing the storage of the
            index
        precision (int): Accuracy of the index. By default the precision is 3
            characters.
        synchronizer (lock.Synchronizer, optional): Write synchronizer

    Return:
        GeoHash: index handler
    """
    result = GeoHash(store, precision, synchronizer)
    result.set_properties()
    return result


def open_geohash(store: storage.AbstractMutableMapping,
                 synchronizer: Optional[lock.Synchronizer] = None) -> GeoHash:
    """Open of a GeoHash index

    Args:
        store (AbstractMutableMapping): Object managing the storage of the index
        synchronizer (lock.Synchronizer, optional): Write synchronizer

    Return:
        GeoHash: index handler
    """
    result = GeoHash(store,
                     synchronizer=synchronizer,
                     **GeoHash.get_properties(store))
    return result


def to_xarray(hashs: numpy.ndarray,
              data: numpy.ndarray) -> xarray.DataArray:
    """Get the XArray grid representing the GeoHash grid.

    Args:
        hashs (numpy.ndarray): Geohash codes
        data (numpy.ndarray): The data associated with the codes provided.
        data (numpy.ndarray): The data associated with the codes provided.

    Return:
        GeoHash: index handler
    """
    if hashs.shape != data.shape:
        raise ValueError(
            "hashs, data could not be broadcast together with shape "
            f"{hashs.shape}, f{data.shape}")
    if hashs.dtype.kind != 'S':
        raise TypeError("hashs must be a string array")
    lon, lat = string.decode(
        string.bounding_boxes(precision=hashs.dtype.itemsize))
    x_axis = core.Axis(numpy.unique(lon), is_circle=True)
    y_axis = core.Axis(numpy.unique(lat))

    dtype = numpy.dtype(type(data[0]))
    if numpy.issubdtype(dtype, numpy.dtype("object")):
        grid = numpy.empty((len(y_axis), len(x_axis)), dtype)
    else:
        grid = numpy.zeros((len(y_axis), len(x_axis)), dtype)

    lon, lat = string.decode(hashs)
    grid[y_axis.find_index(lat), x_axis.find_index(lon)] = data

    return xarray.DataArray(
        grid,
        dims=('lat', 'lon'),
        coords=dict(lon=xarray.DataArray(x_axis,
                                         dims=("lon", ),
                                         attrs=dict(units="degrees_north")),
                    lat=xarray.DataArray(y_axis,
                                         dims=("lat", ),
                                         attrs=dict(units="degrees_east"))))
