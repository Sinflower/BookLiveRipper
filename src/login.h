#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
#include <Download.h>

namespace Ui {
	class Login;
}

class Login : public QDialog
{
		Q_OBJECT

	public:
		explicit Login(QWidget *parent = 0);
		~Login();
		bool CheckSavedLogin();
		QList<QNetworkCookie> GetCookies();
		bool IsGuest();

	protected:
		void keyPressEvent(class QKeyEvent *pEvent);

	signals:
		void loginSuccessful(QList<QNetworkCookie>);

	private slots:
		void on_pushButton_Login_clicked();
		void on_pushButton_Cancel_clicked();
		void setLoadingFrame();

	private:
		bool loginAndElementDisable(QString email, QString password);
		bool login(QString email, QString password);

	private:
		Ui::Login *m_pUi;
		QList<QNetworkCookie> m_cookies;
		class QMovie *m_pLoginGif;
		bool m_guest;
};

#endif // LOGIN_H
