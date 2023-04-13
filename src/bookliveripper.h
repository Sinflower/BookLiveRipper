#ifndef BOOKLIVERIPPER_H
#define BOOKLIVERIPPER_H

#include <QWidget>

namespace Ui {
	class BookLiveRipper;
}

class BookLiveRipper : public QWidget
{
		Q_OBJECT

	public:
		explicit BookLiveRipper(QWidget *parent = 0);
		~BookLiveRipper();

private slots:
	void on_pushButton_Download_clicked();

private:

	private:
		Ui::BookLiveRipper *m_pUi;
		class BookDownloadAndExport *m_pBDAE;
};

#endif // BOOKLIVERIPPER_H
