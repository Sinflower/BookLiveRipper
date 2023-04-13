#include "bookliveripper.h"
#include "ui_bookliveripper.h"

#include "login.h"
#include "bookdownloadandexport.h"

#include <QDebug>

#include <time.h>

// TODO: Add more logging and shit

BookLiveRipper::BookLiveRipper(QWidget *parent) :
	QWidget(parent),
	m_pUi(new Ui::BookLiveRipper),
	m_pBDAE(new BookDownloadAndExport)
{
	m_pUi->setupUi(this);
	qsrand(time(nullptr));
	Login l;
	l.show();
	if(!l.CheckSavedLogin())
	{
		if(l.exec() == QDialog::Rejected)
			exit(0);
	}

	if(l.IsGuest())
		setWindowTitle(QString("%1 (Guest Mode)").arg(windowTitle()));

	m_pBDAE->SetCookies(l.GetCookies());
	raise();
	activateWindow();
}

BookLiveRipper::~BookLiveRipper()
{
	delete m_pUi;
	delete m_pBDAE;
}

/*
 * Private
 */

void BookLiveRipper::on_pushButton_Download_clicked()
{
	QString bookID = QString("%1_%2").arg(m_pUi->lineEdit_TitleID->text()).arg(m_pUi->lineEdit_VolNum->text());
	m_pBDAE->Load(bookID);
	m_pBDAE->exec();
	raise();
	activateWindow();
}
