# DuckDB EEA Reference Grid Function Reference

## Function Index 
**[Scalar Functions](#scalar-functions)**

| Function | Summary |
| --- | --- |
| [`EEA_CoordXY2GridNum`](#eea_coordxy2gridnum) | Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035). |
| [`EEA_GridNum2CoordX`](#eea_gridnum2coordx) | Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code. |
| [`EEA_GridNum2CoordY`](#eea_gridnum2coordy) | Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code. |

----

## Scalar Functions

### EEA_CoordXY2GridNum


#### Signature

```sql
BIGINT EEA_CoordXY2GridNum (col0 BIGINT, col1 BIGINT)
```

#### Description

Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035).

#### Example

```sql
SELECT CoordXY2GridNum();
```

----

### EEA_GridNum2CoordX


#### Signature

```sql
BIGINT EEA_GridNum2CoordX (col0 BIGINT)
```

#### Description

Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.

#### Example

```sql
SELECT EEA_GridNum2CoordX();
```

----

### EEA_GridNum2CoordY


#### Signature

```sql
BIGINT EEA_GridNum2CoordY (col0 BIGINT)
```

#### Description

Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.

#### Example

```sql
SELECT EEA_GridNum2CoordY();
```

----

