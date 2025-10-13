/**
 * Data Ingestion Monitoring API Handlers - Header
 */

#ifndef REGULENS_INGESTION_API_HANDLERS_HPP
#define REGULENS_INGESTION_API_HANDLERS_HPP

#include <string>
#include <map>
#include <libpq-fe.h>

namespace regulens {
namespace ingestion {

std::string get_ingestion_metrics(PGconn* db_conn, const std::map<std::string, std::string>& query_params);
std::string get_quality_checks(PGconn* db_conn, const std::map<std::string, std::string>& query_params);

} // namespace ingestion
} // namespace regulens

#endif // REGULENS_INGESTION_API_HANDLERS_HPP
