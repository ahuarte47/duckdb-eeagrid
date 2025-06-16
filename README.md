# DuckDB EEA Reference Grid Extension

## What is this?

This is an extension for DuckDB that adds support for working with the [EEA Reference Grid System](https://sdi.eea.europa.eu/catalogue/srv/api/records/aac8379a-5c4e-445c-b2ef-23a6a2701ef0/attachments/eea_reference_grid_v1.pdf).

The **EEA Reference Grid** is a standardized spatial grid system used across Europe for environmental data analysis and reporting. It is maintained by the [European Environment Agency](https://www.eea.europa.eu/en) (EEA) and forms the basis for aggregating and exchanging geospatial data in a consistent format:

* `Coordinate Reference System (CRS)`: ETRS89 / LAEA Europe ([EPSG:3035](https://epsg.io/3035)), which minimizes area distortion across Europe. The Geodetic Datum is the European Terrestrial Reference System 1989 (EPSG:6258). The Lambert Azimuthal Equal Area (LAEA) projection is
centred at 10°E, 52°N. Coordinates are based on a false Easting of 4321000 meters, and a false Northing of 3210000 meters.
* `Supported resolutions`: Typically available at 10 km, 1 km, and 100 m resolutions.
* `Structure`: Regular square grid with unique cell codes and identifiers assigned based on position and resolution.
* `Purpose`: Enables harmonized spatial analysis, mapping, and cross-border environmental assessments.

This grid system is widely used in European environmental datasets, including air quality, land use, biodiversity, and climate change indicators.

The extension provides functions to calculate grid cell identifiers (`INT64`) from XY coordinates based on the `EPSG:3035` coordinate reference system, and vice versa. Please see the [function table](docs/functions.md) for the current implementation status.

## How do I get it?

### Building from source

This extension is based on the [DuckDB extension template](https://github.com/duckdb/extension-template).

### Loading from community (TODO)

The DuckDB **EEA Reference Grid extension** is not yet available as a signed [community extension](https://duckdb.org/community_extensions/list_of_extensions) and currently requires the use of the unsigned flag to be loaded.

Our intention is to submit this extension for inclusion in the official DuckDB community extensions registry. If accepted, it will be possible to install and load the extension in any DuckDB instance using the following commands:

```sql
INSTALL eeagrid FROM community;
LOAD eeagrid;
```

## Example Usage

```sql
LOAD eeagrid;

SELECT EEA_CoordXY2GridNum(5078600, 2871400);
----
23090257455218688

SELECT EEA_GridNum2CoordX(23090257455218688);
----
5078600

SELECT EEA_GridNum2CoordY(23090257455218688);
----
2871400

SELECT EEA_GridNumAt100m(23090257455218688);
----
23090257455218688

query I
SELECT EEA_GridNumAt1km(23090257455218688);
----
23090257448665088

query I
SELECT EEA_GridNumAt10km(23090257455218688);
----
23090255284404224
```

### Supported Functions and Documentation

The full list of functions and their documentation is available in the [function reference](docs/functions.md)

## How do I build it?

### Dependencies

You need a recent version of CMake (3.20) and a C++11 compatible compiler.

We also highly recommend that you install [Ninja](https://ninja-build.org) which you can select when building by setting the `GEN=ninja` environment variable.
```
git clone --recurse-submodules https://github.com/ahuarte47/duckdb-eeagrid
cd duckdb-eeagrid
make release
```

You can then invoke the built DuckDB (with the extension statically linked)
```
./build/release/duckdb
```

Please see the Makefile for more options, or the extension template documentation for more details.

### Running the tests

Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:

```sh
make test
```

### Installing the deployed binaries

To install your extension binaries from S3, you will need to do two things. Firstly, DuckDB should be launched with the
`allow_unsigned_extensions` option set to true. How to set this will depend on the client you're using. Some examples:

CLI:
```shell
duckdb -unsigned
```

Python:
```python
con = duckdb.connect(':memory:', config={'allow_unsigned_extensions' : 'true'})
```

NodeJS:
```js
db = new duckdb.Database(':memory:', {"allow_unsigned_extensions": "true"});
```

Secondly, you will need to set the repository endpoint in DuckDB to the HTTP url of your bucket + version of the extension
you want to install. To do this run the following SQL query in DuckDB:
```sql
SET custom_extension_repository='bucket.s3.eu-west-1.amazonaws.com/<your_extension_name>/latest';
```
Note that the `/latest` path will allow you to install the latest extension version available for your current version of
DuckDB. To specify a specific version, you can pass the version instead.

After running these steps, you can install and load your extension using the regular INSTALL/LOAD commands in DuckDB:
```sql
INSTALL eeagrid;
LOAD eeagrid;
```

Enjoy!
