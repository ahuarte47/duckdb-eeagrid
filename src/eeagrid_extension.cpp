#define DUCKDB_EXTENSION_MAIN

#include "eeagrid_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/catalog/catalog_entry/function_entry.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

namespace {

//! Register a function or functionSet in the database and add its metadata.
template <typename FUNC>
static void RegisterFunction(ExtensionLoader &loader, FUNC function, const string &description, const string &example,
                             InsertionOrderPreservingMap<string> &tags) {
	// Register the function
	loader.RegisterFunction(function);
	auto &db = loader.GetDatabaseInstance();

	auto &catalog = Catalog::GetSystemCatalog(db);
	auto transaction = CatalogTransaction::GetSystemTransaction(db);
	auto &schema = catalog.GetSchema(transaction, DEFAULT_SCHEMA);
	auto catalog_entry = schema.GetEntry(transaction, CatalogType::SCALAR_FUNCTION_ENTRY, function.name);
	if (!catalog_entry) {
		// This should not happen, we just registered the function
		throw InternalException("Function with name \"%s\" not found.", function.name.c_str());
	}

	auto &func_entry = catalog_entry->template Cast<FunctionEntry>();

	// Fill a function description and add it to the function entry
	FunctionDescription func_description;
	if (!description.empty()) {
		func_description.description = description;
	}
	if (!example.empty()) {
		func_description.examples.push_back(example);
	}
	for (const auto &tag : tags) {
		func_entry.tags.insert(tag.first, tag.second);
	}

	func_entry.descriptions.push_back(func_description);
}

//======================================================================================================================
// EEA_Grid
//======================================================================================================================

struct EEA_Grid {

	//! Returns the sign of a value: 1 for positive, -1 for negative, and 0 for zero.
	inline static int Sign(int64_t val) {
		return (val > 0) - (val < 0);
	}

	//! Extracts a hexadecimal nibble from a specific position in the grid number and scales it.
	inline static int32_t ExtractNibbleAndScale(int64_t grid_num, int64_t hex_mask, int32_t factor) {
		// Extract the nibble (4 bits) at the specified position,
		// divide by hex_mask to shift right, then mask with 0x0f to get lower 4 bits.
		int32_t nibble = (static_cast<int32_t>(grid_num / hex_mask) & 0x0f);

		// Ensure the nibble value is a valid decimal digit (0-9),
		// Hex values A-F (10-15) are clamped to 9 to maintain decimal representation.
		nibble = (nibble <= 9) ? nibble : 9;

		return nibble * factor;
	}

	//! Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035).
	inline static void CoordXY2GridNum(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 2);

		constexpr int64_t p13 = int64_t(1) << 52; // 16^13 = 2^52
		constexpr int64_t p12 = int64_t(1) << 48; // 16^12 = 2^48
		constexpr int64_t p11 = int64_t(1) << 44; // 16^11 = 2^44
		constexpr int64_t p10 = int64_t(1) << 40; // 16^10 = 2^40
		constexpr int64_t p9 = int64_t(1) << 36;  // 16^9  = 2^36
		constexpr int64_t p8 = int64_t(1) << 32;  // ...
		constexpr int64_t p7 = int64_t(1) << 28;
		constexpr int64_t p6 = int64_t(1) << 24;
		constexpr int64_t p5 = int64_t(1) << 20;
		constexpr int64_t p4 = int64_t(1) << 16;
		constexpr int64_t p3 = int64_t(1) << 12;
		constexpr int64_t p2 = int64_t(1) << 8;
		constexpr int64_t p1 = int64_t(1) << 4;
		constexpr int64_t p0 = 1;

		BinaryExecutor::Execute<int64_t, int64_t, int64_t>(args.data[0], args.data[1], result, args.size(),
		                                                   [&](int64_t x, int64_t y) {
			                                                   int64_t X = std::abs(x);
			                                                   int64_t Y = std::abs(y);
			                                                   int sx = EEA_Grid::Sign(x);
			                                                   int sy = EEA_Grid::Sign(y);

			                                                   int64_t grid_num = 0;
			                                                   grid_num += (x / 1000000) * p13;
			                                                   grid_num += (y / 1000000) * p12;
			                                                   grid_num += ((X / 100000) % 10) * p11 * sx;
			                                                   grid_num += ((Y / 100000) % 10) * p10 * sy;
			                                                   grid_num += ((X / 10000) % 10) * p9 * sx;
			                                                   grid_num += ((Y / 10000) % 10) * p8 * sy;
			                                                   grid_num += ((X / 1000) % 10) * p7 * sx;
			                                                   grid_num += ((Y / 1000) % 10) * p6 * sy;
			                                                   grid_num += ((X / 100) % 10) * p5 * sx;
			                                                   grid_num += ((Y / 100) % 10) * p4 * sy;
			                                                   grid_num += ((X / 10) % 10) * p3 * sx;
			                                                   grid_num += ((Y / 10) % 10) * p2 * sy;
			                                                   grid_num += (X % 10) * p1 * sx;
			                                                   grid_num += (Y % 10) * p0 * sy;
			                                                   return grid_num;
		                                                   });
	}

	//! Defines a hexadecimal divisor and decimal place factor entry.
	struct MappingEntry {
		int64_t hex_mask;
		int32_t factor;
		constexpr MappingEntry(int64_t h, int32_t f) : hex_mask(h), factor(f) {
		}
	};

	//! Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.
	inline static void GridNum2CoordX(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		// Mapping array: {hexadecimal divisor for position extraction, decimal place factor}
		// Each pair represents: (hex_position_divisor, decimal_place_value)
		// Positions correspond to hexadecimal shifts (13, 11, 9, 7, 5, 3, 1)
		constexpr MappingEntry mapping_X[] = {
		    {0x10000000000000LL, 1000000}, // Position 13: millions place
		    {0x00100000000000LL, 100000},  // Position 11: hundred thousands place
		    {0x00001000000000LL, 10000},   // Position 9:  ten thousands place
		    {0x00000010000000LL, 1000},    // Position 7:  thousands place
		    {0x00000000100000LL, 100},     // Position 5:  hundreds place
		    {0x00000000001000LL, 10},      // Position 3:  tens place
		    {0x00000000000010LL, 1}        // Position 1:  ones place
		};

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(), [&](int64_t grid_num) {
			int64_t x = 0;

			// Process each position-factor pair in descending order of significance
			for (const auto &entry : mapping_X) {
				auto hex_mask = entry.hex_mask;
				auto factor = entry.factor;

				// Add the contribution of the digit to the final coordinate
				x += EEA_Grid::ExtractNibbleAndScale(grid_num, hex_mask, factor);
			}

			return x;
		});
	}

	//! Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code and
	//! resolution.
	inline static void GridNum2CoordXAtRes(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 2);

		// Mapping array: {hexadecimal divisor for position extraction, decimal place factor}
		// Each pair represents: (hex_position_divisor, decimal_place_value)
		// Positions correspond to hexadecimal shifts (13, 11, 9, 7, 5, 3, 1)
		constexpr MappingEntry mapping_X[] = {
		    {0x10000000000000LL, 1000000}, // Position 13: millions place
		    {0x00100000000000LL, 100000},  // Position 11: hundred thousands place
		    {0x00001000000000LL, 10000},   // Position 9:  ten thousands place
		    {0x00000010000000LL, 1000},    // Position 7:  thousands place
		    {0x00000000100000LL, 100},     // Position 5:  hundreds place
		    {0x00000000001000LL, 10},      // Position 3:  tens place
		    {0x00000000000010LL, 1}        // Position 1:  ones place
		};

		BinaryExecutor::Execute<int64_t, int64_t, int64_t>(
		    args.data[0], args.data[1], result, args.size(), [&](int64_t grid_num, int resolution) {
			    int64_t x = 0;

			    // Validate resolution - must be a power of ten up to 1,000,000
			    switch (resolution) {
			    case 10:
			    case 100:
			    case 1000:
			    case 10000:
			    case 100000:
			    case 1000000:
				    break;
			    default:
				    throw InvalidInputException("Invalid resolution: must be a power of ten up to 1,000,000");
			    }

			    // Process each position-factor pair in descending order of significance
			    for (const auto &entry : mapping_X) {
				    auto hex_mask = entry.hex_mask;
				    auto factor = entry.factor;

				    // Add the contribution of the digit to the final coordinate,
				    // only if current factor is within resolution bounds
				    if (resolution <= factor) {
					    x += EEA_Grid::ExtractNibbleAndScale(grid_num, hex_mask, factor);
				    }
			    }

			    return x;
		    });
	}

	//! Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.
	inline static void GridNum2CoordY(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		// Mapping array: {hexadecimal divisor for position extraction, decimal place factor}
		// Each pair represents: (hex_position_divisor, decimal_place_value)
		// Positions correspond to hexadecimal shifts (12, 10, 8, 6, 4, 2, 0) - odd positions
		constexpr MappingEntry mapping_Y[] = {
		    {0x1000000000000LL, 1000000}, // Position 12: millions place
		    {0x0010000000000LL, 100000},  // Position 10: hundred thousands place
		    {0x0000100000000LL, 10000},   // Position 8:  ten thousands place
		    {0x0000001000000LL, 1000},    // Position 6:  thousands place
		    {0x0000000010000LL, 100},     // Position 4:  hundreds place
		    {0x0000000000100LL, 10},      // Position 2:  tens place
		    {0x0000000000001LL, 1}        // Position 0:  ones place
		};

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(), [&](int64_t grid_num) {
			int64_t y = 0;

			// Process each position-factor pair in descending order of significance
			for (const auto &entry : mapping_Y) {
				auto hex_mask = entry.hex_mask;
				auto factor = entry.factor;

				// Add the contribution of the digit to the final coordinate
				y += EEA_Grid::ExtractNibbleAndScale(grid_num, hex_mask, factor);
			}

			return y;
		});
	}

	//! Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code and
	//! resolution.
	inline static void GridNum2CoordYAtRes(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 2);

		// Mapping array: {hexadecimal divisor for position extraction, decimal place factor}
		// Each pair represents: (hex_position_divisor, decimal_place_value)
		// Positions correspond to hexadecimal shifts (12, 10, 8, 6, 4, 2, 0) - odd positions
		constexpr MappingEntry mapping_Y[] = {
		    {0x1000000000000LL, 1000000}, // Position 12: millions place
		    {0x0010000000000LL, 100000},  // Position 10: hundred thousands place
		    {0x0000100000000LL, 10000},   // Position 8:  ten thousands place
		    {0x0000001000000LL, 1000},    // Position 6:  thousands place
		    {0x0000000010000LL, 100},     // Position 4:  hundreds place
		    {0x0000000000100LL, 10},      // Position 2:  tens place
		    {0x0000000000001LL, 1}        // Position 0:  ones place
		};

		BinaryExecutor::Execute<int64_t, int64_t, int64_t>(
		    args.data[0], args.data[1], result, args.size(), [&](int64_t grid_num, int resolution) {
			    int64_t y = 0;

			    // Validate resolution - must be a power of ten up to 1,000,000
			    switch (resolution) {
			    case 10:
			    case 100:
			    case 1000:
			    case 10000:
			    case 100000:
			    case 1000000:
				    break;
			    default:
				    throw InvalidInputException("Invalid resolution: must be a power of ten up to 1,000,000");
			    }

			    // Process each position-factor pair in descending order of significance
			    for (const auto &entry : mapping_Y) {
				    auto hex_mask = entry.hex_mask;
				    auto factor = entry.factor;

				    // Add the contribution of the digit to the final coordinate,
				    // only if current factor is within resolution bounds
				    if (resolution <= factor) {
					    y += EEA_Grid::ExtractNibbleAndScale(grid_num, hex_mask, factor);
				    }
			    }

			    return y;
		    });
	}

	//! Returns the grid code at 100 m resolution given an EEA reference grid code.
	inline static void GridNumAt100m(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(),
		                                         [&](int64_t grid_num) { return (grid_num & 1152921504606781440LL); });
	}

	//! Returns the grid code at 1 km resolution given an EEA reference grid code.
	inline static void GridNumAt1km(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(),
		                                         [&](int64_t grid_num) { return (grid_num & 1152921504590069760LL); });
	}

	//! Returns the grid code at 10 km resolution given an EEA reference grid code.
	inline static void GridNumAt10km(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(),
		                                         [&](int64_t grid_num) { return (grid_num & 1152921500311879680LL); });
	}

	static void Register(ExtensionLoader &loader) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "eeagrid");
		tags.insert("category", "scalar");

		RegisterFunction<ScalarFunction>(loader,
		                                 ScalarFunction("EEA_CoordXY2GridNum",
		                                                {LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::BIGINT,
		                                                EEA_Grid::CoordXY2GridNum),
		                                 "Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035).",
		                                 "SELECT CoordXY2GridNum(5078600, 2871400); -> 23090257455218688", tags);

		ScalarFunctionSet gridNum2CoordX("EEA_GridNum2CoordX");
		gridNum2CoordX.AddFunction(
		    ScalarFunction({LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNum2CoordX));
		gridNum2CoordX.AddFunction(ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::BIGINT,
		                                          EEA_Grid::GridNum2CoordXAtRes));

		RegisterFunction<ScalarFunctionSet>(
		    loader, gridNum2CoordX,
		    "Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code, "
		    "optionally truncating the value to a specified resolution.",
		    "SELECT EEA_GridNum2CoordX(23090257455218688); -> 5078600", tags);

		ScalarFunctionSet gridNum2CoordY("EEA_GridNum2CoordY");
		gridNum2CoordY.AddFunction(
		    ScalarFunction({LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNum2CoordY));
		gridNum2CoordY.AddFunction(ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::BIGINT,
		                                          EEA_Grid::GridNum2CoordYAtRes));

		RegisterFunction<ScalarFunctionSet>(
		    loader, gridNum2CoordY,
		    "Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code, "
		    "optionally truncating the value to a specified resolution.",
		    "SELECT EEA_GridNum2CoordY(23090257455218688); -> 2871400", tags);

		RegisterFunction<ScalarFunction>(
		    loader,
		    ScalarFunction("EEA_GridNumAt100m", {LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNumAt100m),
		    "Returns the Grid code at 100 m resolution given an EEA reference Grid code.",
		    "SELECT EEA_GridNumAt100m(23090257455218688); -> 23090257455218688", tags);

		RegisterFunction<ScalarFunction>(
		    loader,
		    ScalarFunction("EEA_GridNumAt1km", {LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNumAt1km),
		    "Returns the Grid code at 1 km resolution given an EEA reference Grid code.",
		    "SELECT EEA_GridNumAt1km(23090257455218688); -> 23090257448665088", tags);

		RegisterFunction<ScalarFunction>(
		    loader,
		    ScalarFunction("EEA_GridNumAt10km", {LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNumAt10km),
		    "Returns the Grid code at 10 km resolution given an EEA reference Grid code.",
		    "SELECT EEA_GridNumAt10km(23090257455218688); -> 23090255284404224", tags);
	}
};

} // namespace

// ######################################################################################################################
//  Register Functions & Extension metadata
// ######################################################################################################################

static void LoadInternal(ExtensionLoader &loader) {
	EEA_Grid::Register(loader);
}

void EeagridExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string EeagridExtension::Name() {
	return "eeagrid";
}

std::string EeagridExtension::Version() const {
#ifdef EXT_VERSION_EEAGRID
	return EXT_VERSION_EEAGRID;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(eeagrid, loader) {
	duckdb::LoadInternal(loader);
}
}
