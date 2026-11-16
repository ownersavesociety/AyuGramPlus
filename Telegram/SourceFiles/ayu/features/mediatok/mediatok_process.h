// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace MediaTok {

bool RunHiddenProcess(
	const QString &executable,
	const QStringList &arguments,
	const QString &workingDirectory,
	int timeoutMs,
	int *exitCode = nullptr);

} // namespace MediaTok
