#ifndef BOOKDOWNLOADANDEXPORT_H
#define BOOKDOWNLOADANDEXPORT_H

#include <QDialog>
#include <Download.h>
#include <QFutureWatcher>

namespace Ui
{
	class BookDownloadAndExport;
}


enum ExportTypes
{
	TXT_SPLIT,
	TXT,
	HTML,
	DOCX
};

class BookDownloadAndExport : public QDialog
{
		Q_OBJECT

	public:
		explicit BookDownloadAndExport(QWidget *parent = 0);
		~BookDownloadAndExport();
		void SetCookies(QList<QNetworkCookie> cookies);
		void Load(QString bookID);

	protected:
		void keyPressEvent(class QKeyEvent *pEvent);

	signals:
		void updateStatus(QString);

	private slots:
		void on_pushButton_Done_clicked();
		void on_pushButton_Cancel_clicked();
		void updateStatusMsg(QString);
		void exportBook();

	private:
		void load(QString bookID);

		QString genDecodeKey();
		void loadConfig(QString bookID);
		void loadBook();
		void loadImage(QString name);
		void loadB64Image(QString name);

		void updateProgress(int min, int max, int val);

		void resetAll();
		QString getExportString(int type);
		bool isFontBold(QString str);

	private:
		Ui::BookDownloadAndExport *m_pUi;
		QString m_ptbl;
		QString m_ctbl;
		QString m_stbl;
		QString m_ttbl;
		QString m_decodeKey;
		QString m_bookID;
		QString m_pID;
		QString m_cServer;
		QString m_book;
		bool m_trial;
		bool m_configDone;
		class Decoder *m_pDecoder;
		QSet<QString> m_images;

		QString m_decodedBookHtml;
		QStringList m_knownHeaderLinks;
		QHash<QString, QString> m_headerLinkValueHash;

		QList<QNetworkCookie> m_cookies;
		QFutureWatcher<void> *m_pWatcher;

		class QMenu *m_pExportMenu;
		class QAction *m_pTxtSplit;
		class QAction *m_pTxt;
		class QAction *m_pHtml;
		class QAction *m_pDocx;
};

#endif // BOOKDOWNLOADANDEXPORT_H
