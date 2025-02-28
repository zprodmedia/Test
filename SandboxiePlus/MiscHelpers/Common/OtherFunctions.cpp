#include "stdafx.h"
#include "OtherFunctions.h"

//////////////////////////////////////////////////////////////////////////////////////////
// File system functions
// 

QString ReadFileAsString(const QString& filename)
{
	QFile file(filename);
	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QTextStream stream(&file);
		QString content = stream.readAll();
		file.close();
		content.remove("\r").replace("\n","\r\n");
		return content;
	}
	return "";
}

bool WriteStringToFile(const QString& filename, const QString& content)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;
	QTextStream out(&file);
	out << QString(content).remove("\r");
	file.close();
	return true;
}

bool CreateDir(const QString& path)
{
	QDir dir(path);
	if (dir.exists())
		return true;
	return dir.mkpath(path);
}

bool DeleteDir(const QString& path, bool bEmpty)
{
	bool error = false;
	QDir delDir(path);
	QStringList fileNames = delDir.entryList();
	foreach (const QString& name, fileNames)
	{
		if (name.compare(".") == 0 || name.compare("..") == 0)
			continue;
		QString src = path;
		src = src.append("/%1").arg(name);
		QFileInfo srcInfo(src);
		if (srcInfo.isDir())
		{
			if(!DeleteDir(src, bEmpty))
				error = true;
		}
		else if(bEmpty)
			error = true;
		else
		{
			if(!QFile::remove(src))
				error = true;
		}
	}
	if (!error)
	{
		if(!QDir().rmdir(path))
			error = true;
	}
	return !error;
}

bool CopyDir(const QString& srcDirPath, const QString& destDirPath, bool bMove)
{
	QDir srcDir(srcDirPath);
	if (!srcDir.exists())
		return false;

	QDir destDir(destDirPath);
	if (!destDir.exists() && !destDir.mkdir(destDirPath))
		return false;

	QStringList fileNames = srcDir.entryList();
	foreach (const QString& name, fileNames)
	{
		if (name.compare(".") == 0 || name.compare("..") == 0)
			continue;
		QString src = srcDirPath;
		src = src + "/" + name;
		QString dst = destDirPath;
		QFileInfo srcInfo(src);
		if (srcInfo.isDir())
		{
			dst = dst.append("/" + srcInfo.baseName());
			CopyDir(src, dst, bMove);
		}
		else
		{
			dst = dst.append("/%1").arg(name);
			if(bMove)
				QFile::rename(src, dst);
			else
				QFile::copy(src, dst);
		}
	}
	if(bMove)
		QDir().rmdir(srcDirPath);
	return true;
}

QStringList	ListDir(const QString& srcDirPath, const QStringList& NameFilter, bool bAndSubDirs)
{
	QStringList FileList;
	QDir srcDir(srcDirPath);
	if (!srcDir.exists())
		return FileList;

	QStringList Files = !NameFilter.isEmpty() ? srcDir.entryList(NameFilter, QDir::Files | QDir::Hidden | QDir::System) : srcDir.entryList(QDir::Files | QDir::Hidden | QDir::System);
	foreach (const QString& FileName, Files)
		FileList.append(FileName);

	if(!bAndSubDirs)
		return FileList;

	QStringList Dirs = srcDir.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
	foreach (const QString& DirName, Dirs)
	{
		//if (DirName.compare(".") == 0 || DirName.compare("..") == 0)
		//	continue;
		QStringList SubFiles = ListDir(srcDirPath + "/" + DirName, NameFilter);
		foreach (const QString& FileName, SubFiles)
			FileList.append(DirName + "/" + FileName);

	}
	return FileList;
}

bool SafeRemove(const QString& FilePath)
{
	bool bRemoved = false;
	for(int i=0; i < 100; i++) // try for 10 sec
	{
		bRemoved = QFile::remove(FilePath);
		if(bRemoved)
			break;
		QThread::currentThread()->msleep(100);
	}
	return bRemoved;
}

QString GetRelativeSharedPath(const QString& fullPath, const QStringList& shared, QString& rootPath)
{
	QString relativePath = fullPath;
	bool bFound = false;
	foreach (QString curShared, shared)
	{
		curShared = curShared.replace("\\", "/");

		int length = curShared.length();
		if (length == 0)
			continue;
		QFileInfo fileInfo = QFileInfo(fullPath);
		if (!fileInfo.isDir())
		{
			QString curDir = curShared.left(curShared.lastIndexOf('/'));
			length = curDir.length();
			if (fullPath.left(length).compare(curDir) == 0) {
				rootPath = relativePath.left(length+1);
				relativePath.remove(0, length+1);
				return relativePath;
			}
		}
		else
		{
			if (fullPath.left(length).compare(curShared) == 0) {
				rootPath = relativePath.left(length);
				relativePath.remove(0, length);
				return relativePath;
			}
		}
	}
	int length = relativePath.lastIndexOf("/");
	rootPath = relativePath.left(length+1);
	relativePath.remove(0, length+1);
	return relativePath;
}

QString NameOfFile(const QString& FileName)
{
	int Begin = FileName.lastIndexOf("/") + 1;
	int End = FileName.lastIndexOf(".");
	if(End!= -1)
		return FileName.mid(Begin, End - Begin);
	return FileName.mid(Begin);
}



QIcon IconAddOverlay(const QIcon& Icon, const QString& Name, int Size)
{
	QPixmap overlay = QPixmap(Name).scaled(Size, Size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	QPixmap base = Icon.pixmap(32, 32).scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QPixmap result(base.width(), base.height());
	result.fill(Qt::transparent); // force alpha channel
	QPainter painter(&result);
	painter.drawPixmap(0, 0, base);

	painter.drawPixmap(32 - Size, 32 - Size, overlay);
	return QIcon(result);
}
