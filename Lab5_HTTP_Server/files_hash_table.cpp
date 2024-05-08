#include "files_hash_table.h"

const std::string error404_page_path = "error_page.html";

std::unordered_map<std::string, std::string> file_to_content_umap = {
	{ "index.html", "" },
	{ "second_page.html", "" },
	{ error404_page_path, "" },
};