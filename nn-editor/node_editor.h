#pragma once

namespace example
{
void NodeEditorInitialize(int, char**);
void NodeEditorShow();
void NodeEditorShutdown();
void NodeEditorSetExit(int);
void NodeEditorQueueOpenFile(const char* path);
} // namespace example