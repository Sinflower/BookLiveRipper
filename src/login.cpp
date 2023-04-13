#include "login.h"
#include "ui_login.h"

#include <QSettings>
#include <QMessageBox>
#include <QKeyEvent>
#include <QMovie>


#define TOKEN_REGEX "<input .*token\" value=\"([^\"]+)\">"

Login::Login(QWidget *parent) :
	QDialog(parent),
	m_pUi(new Ui::Login),
	m_pLoginGif(new QMovie(":/icons/login")),
	m_guest(false)
{
	m_pUi->setupUi(this);
	connect(m_pLoginGif, SIGNAL(frameChanged(int)), this, SLOT(setLoadingFrame()), Qt::UniqueConnection);
}

Login::~Login()
{
	delete m_pUi;
	m_pLoginGif;
}


// Disable dialog close when escape is pressed
void Login::keyPressEvent(QKeyEvent *pEvent)
{
	if(pEvent->key() != Qt::Key_Escape)
		QDialog::keyPressEvent(pEvent);
}


bool Login::CheckSavedLogin()
{
	QSettings settings("settings.ini", QSettings::IniFormat);
	if(settings.contains("email") && settings.contains("password"))
	{
		m_pUi->lineEdit_email->setText(settings.value("email").toString());
		m_pUi->lineEdit_password->setText("yeahnicetry");
		if(loginAndElementDisable(settings.value("email").toString(), settings.value("password").toString()))
		{
			emit loginSuccessful(m_cookies);
			return true;
		}
	}

	return false;
}

QList<QNetworkCookie> Login::GetCookies()
{
	return m_cookies;
}

bool Login::IsGuest()
{
	return m_guest;
}

void Login::on_pushButton_Login_clicked()
{
	QString email = m_pUi->lineEdit_email->text();
	QString password = m_pUi->lineEdit_password->text();

	if(email.isEmpty() || password.isEmpty())
	{
		QMessageBox msgBox;
		msgBox.setText("Missing input<br>E-mail address or password not set.");
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.exec();
		return;
	}

	if(!loginAndElementDisable(email, password))
	{
		QMessageBox msgBox;
		msgBox.setText("Login error<br>Invalid E-mail address or password.");
		msgBox.setStandardButtons(QMessageBox::Ok);
		msgBox.setDefaultButton(QMessageBox::Ok);
		msgBox.exec();
		return;
	}

	if(m_pUi->checkBox_SaveLogin->isChecked())
	{
		// guest:guest is the login for just trial downloads
		if(email != "guest" && password != "guest")
		{
			QSettings settings("settings.ini", QSettings::IniFormat);
			settings.setValue("email", email);
			settings.setValue("password", password);
		}
	}


	emit loginSuccessful(m_cookies);
	accept();
}

void Login::on_pushButton_Cancel_clicked()
{
	reject();
}

void Login::setLoadingFrame()
{
	m_pUi->pushButton_Login->setIcon(QIcon(m_pLoginGif->currentPixmap()));
}


/*
 * Private
 */

bool Login::loginAndElementDisable(QString email, QString password)
{
	m_pUi->pushButton_Login->setEnabled(false);
	m_pUi->pushButton_Cancel->setEnabled(false);
	m_pUi->lineEdit_email->setEnabled(false);
	m_pUi->lineEdit_password->setEnabled(false);
	m_pLoginGif->start();
	m_pUi->pushButton_Login->repaint();

	bool loginSuccess = login(email, password);

	m_pLoginGif->stop();
	m_pUi->pushButton_Login->setIcon(QIcon());
	m_pUi->pushButton_Login->setEnabled(true);
	m_pUi->pushButton_Cancel->setEnabled(true);
	m_pUi->lineEdit_email->setEnabled(true);
	m_pUi->lineEdit_password->setEnabled(true);

	return loginSuccess;
}

bool Login::login(QString email, QString password)
{
	m_guest = false;

	qDebug("[%s]: Trying to login...", __func__);
	QString page = Download::Get_sync("https://booklive.jp/login/index", QNetworkProxy::DefaultProxy, &m_cookies, COOKIE_GET);

	QRegExp rx(TOKEN_REGEX);

	if(!page.contains(rx))
	{
		qDebug("[%s]: Index page does not contain token value - Internal changes?", __func__);
		exit(-1);
	}

	rx.indexIn(page);

	if(rx.cap(1).isEmpty())
	{
		qDebug("[%s]: Got a regex hit but no token.", __func__);
		qDebug("[%s]: Regex hit: %s", __func__, qPrintable(rx.cap(0)));
		exit(-1);
	}

	QString postData = QString("token=%1&mail_addr=%2&pswd=%3").arg(rx.cap(1)).arg(email).arg(password);

    // .... They just check whether or not the cookie is set, not its content
	// well ok, the content is just a random number hashed with MD5 but still,
	// they don't even check whether or not it is a valid MD5 value
	m_cookies.append(QNetworkCookie("BL_TRACK", "..."));

	// guest:guest is the login for just trial downloads
	if(email == "guest" || password == "guest")
	{
		m_guest = true;
		return true;
	}

	Download::SetStaticReferer("https://booklive.jp/login?from=firstcpn");
	Download::IgnoreRedirect(true);
	Download::Post_sync("https://booklive.jp/login/index", postData, QNetworkProxy::DefaultProxy, "application/x-www-form-urlencoded", &m_cookies, COOKIE_GET | COOKIE_SET);
	Download::IgnoreRedirect(false);

//	qDebug() << m_cookies;

	// Check for login cookie BL_LI
	foreach(QNetworkCookie cookie, m_cookies)
	{
		if(cookie.name() == "BL_LI")
		{
			qDebug("[%s]: Login successful", __func__);
			return true;
		}
	}

	qDebug("[%s]: Login failed", __func__);
	return false;
}
