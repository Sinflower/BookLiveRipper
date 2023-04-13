#include "bookliveripper.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	BookLiveRipper w;
	w.show();

	return a.exec();
}
