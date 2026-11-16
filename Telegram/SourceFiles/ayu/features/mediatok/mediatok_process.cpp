// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_process.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <QtCore/QProcess>
#endif

namespace MediaTok {

#ifdef Q_OS_WIN
namespace {

QString QuoteWinArg(const QString &arg) {
	if (!arg.isEmpty()
		&& !arg.contains(' ')
		&& !arg.contains('\t')
		&& !arg.contains('"')) {
		return arg;
	}
	QString out = QStringLiteral("\"");
	int backslashes = 0;
	for (const QChar ch : arg) {
		if (ch == '\\') {
			++backslashes;
		} else if (ch == '"') {
			out.append(QString(backslashes * 2 + 1, '\\'));
			out.append('"');
			backslashes = 0;
		} else {
			if (backslashes > 0) {
				out.append(QString(backslashes, '\\'));
				backslashes = 0;
			}
			out.append(ch);
		}
	}
	if (backslashes > 0) {
		out.append(QString(backslashes * 2, '\\'));
	}
	out.append('"');
	return out;
}

} // namespace
#endif

bool RunHiddenProcess(
		const QString &executable,
		const QStringList &arguments,
		const QString &workingDirectory,
		int timeoutMs,
		int *exitCode) {
#ifdef Q_OS_WIN
	QStringList allParts;
	allParts.reserve(arguments.size() + 1);
	allParts.push_back(QuoteWinArg(executable));
	for (const auto &arg : arguments) {
		allParts.push_back(QuoteWinArg(arg));
	}
	std::wstring cmdLine = allParts.join(' ').toStdWString();
	std::wstring workDir = workingDirectory.toStdWString();

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi{};
	const BOOL ok = CreateProcessW(
		nullptr,
		cmdLine.data(),
		nullptr,
		nullptr,
		FALSE,
		CREATE_NO_WINDOW,
		nullptr,
		workDir.empty() ? nullptr : workDir.c_str(),
		&si,
		&pi);
	if (!ok) {
		if (exitCode) {
			*exitCode = -1;
		}
		return false;
	}

	const DWORD waitRes = WaitForSingleObject(
		pi.hProcess,
		timeoutMs > 0 ? static_cast<DWORD>(timeoutMs) : INFINITE);
	if (waitRes == WAIT_TIMEOUT) {
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		if (exitCode) {
			*exitCode = -2;
		}
		return false;
	}

	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	if (exitCode) {
		*exitCode = static_cast<int>(code);
	}
	return (code == 0);
#else
	QProcess proc;
	if (!workingDirectory.isEmpty()) {
		proc.setWorkingDirectory(workingDirectory);
	}
	proc.start(executable, arguments);
	if (!proc.waitForStarted(10000)) {
		if (exitCode) {
			*exitCode = -1;
		}
		return false;
	}
	if (!proc.waitForFinished(timeoutMs)) {
		proc.kill();
		if (exitCode) {
			*exitCode = -2;
		}
		return false;
	}
	if (exitCode) {
		*exitCode = proc.exitCode();
	}
	return (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
#endif
}

} // namespace MediaTok
