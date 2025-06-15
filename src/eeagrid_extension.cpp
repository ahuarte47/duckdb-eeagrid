#define DUCKDB_EXTENSION_MAIN

#include "eeagrid_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/catalog/catalog_entry/function_entry.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

namespace {

//! Register a function in the database and add its metadata.
static void RegisterFunction(DatabaseInstance &db, ScalarFunction function, const string &description,
                             const string &example, InsertionOrderPreservingMap<string> &tags) {
	// Register the function
	ExtensionUtil::RegisterFunction(db, function);

	auto &catalog = Catalog::GetSystemCatalog(db);
	auto transaction = CatalogTransaction::GetSystemTransaction(db);
	auto &schema = catalog.GetSchema(transaction, DEFAULT_SCHEMA);
	auto catalog_entry = schema.GetEntry(transaction, CatalogType::SCALAR_FUNCTION_ENTRY, function.name);
	if (!catalog_entry) {
		// This should not happen, we just registered the function
		throw InternalException("Function with name \"%s\" not found.", function.name.c_str());
	}

	auto &func_entry = catalog_entry->Cast<FunctionEntry>();

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

	//! Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035).
	inline static void CoordXY2GridNum(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 2);

		BinaryExecutor::Execute<int64_t, int64_t, int64_t>(args.data[0], args.data[1], result, args.size(),
		                                                   [&](int64_t x, int64_t y) { return 0; });
	}

	//! Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.
	inline static void GridNum2CoordX(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(),
		                                         [&](int64_t grid_num) { return 0; });
	}

	//! Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.
	inline static void GridNum2CoordY(DataChunk &args, ExpressionState &state, Vector &result) {
		D_ASSERT(args.data.size() == 1);

		UnaryExecutor::Execute<int64_t, int64_t>(args.data[0], result, args.size(),
		                                         [&](int64_t grid_num) { return 0; });
	}

	static void Register(DatabaseInstance &db) {

		InsertionOrderPreservingMap<string> tags;
		tags.insert("ext", "eeagrid");
		tags.insert("category", "scalar");

		RegisterFunction(db,
		                 ScalarFunction("EEA_CoordXY2GridNum", {LogicalType::BIGINT, LogicalType::BIGINT},
		                                LogicalType::BIGINT, EEA_Grid::CoordXY2GridNum),
		                 "Returns the EEA Reference Grid code to a given XY coordinate (EPSG:3035).",
		                 "SELECT CoordXY2GridNum();", tags);

		RegisterFunction(
		    db,
		    ScalarFunction("EEA_GridNum2CoordX", {LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNum2CoordX),
		    "Returns the X-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.",
		    "SELECT EEA_GridNum2CoordX();", tags);

		RegisterFunction(
		    db,
		    ScalarFunction("EEA_GridNum2CoordY", {LogicalType::BIGINT}, LogicalType::BIGINT, EEA_Grid::GridNum2CoordY),
		    "Returns the Y-coordinate (EPSG:3035) of the grid cell corresponding to a given EEA Reference Grid code.",
		    "SELECT EEA_GridNum2CoordY();", tags);
	}
};

} // namespace

// ######################################################################################################################
//  Register Functions & Extension metadata
// ######################################################################################################################

static void LoadInternal(DatabaseInstance &instance) {
	EEA_Grid::Register(instance);
}

void EeagridExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
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

DUCKDB_EXTENSION_API void eeagrid_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::EeagridExtension>();
}

DUCKDB_EXTENSION_API const char *eeagrid_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
