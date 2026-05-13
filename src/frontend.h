#pragma once

void frontend_send_html(void (*sendChunk)(const char*),
                        const char* storage_info,
                        const char* file_list);
