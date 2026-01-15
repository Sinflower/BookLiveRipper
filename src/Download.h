#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#ifndef CONSOLE_ONLY
#include <QListWidget>
#endif

//------------DIR---------------------
#include <QDir>

//------------NETWORK-----------------
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkAccessManager>

//------------STUFF-------------------
#include <QQueue>

#ifndef CONSOLE_ONLY
#include <QProgressBar>
#endif

#include <QRegularExpression>
#include <QHash>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <QTimerEvent>
#include <QByteArray>
#include <QMap>

#define USER_AGENT "User-Agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/114.0"

enum DOWNLOAD_MODE
{
	POST,
	GET,
	HEAD,
	PUT
};

enum COOKIE_MODE
{
	COOKIE_NON = 0,
	COOKIE_SET = 1 << 0,
	COOKIE_GET = 1 << 1
};


inline COOKIE_MODE operator|(COOKIE_MODE a, COOKIE_MODE b)
{
	return static_cast<COOKIE_MODE>(static_cast<int>(a) | static_cast<int>(b));
}

#define KILL_SET_TIMER(TIMERID) { \
	if(TIMERID != -1) \
{ \
	killTimer(TIMERID); \
	TIMERID = -1; \
	} }

template<class T>
struct StaticMemberContainer
{
#ifndef CONSOLE_ONLY
		static QProgressBar *m_pStaticProgress;
#endif
		static bool m_ignoreRedirect;
		static QString m_staticReferer;
		static int m_staticTimeout;
		static QString m_lastError;
		static QMap<QByteArray, QByteArray> m_staticRawHeader;
};


#ifndef CONSOLE_ONLY
template<class QByteArray>
QProgressBar * StaticMemberContainer<QByteArray>::m_pStaticProgress = nullptr;
#endif

template<class QByteArray>
bool StaticMemberContainer<QByteArray>::m_ignoreRedirect = false;

template<class QByteArray>
QString StaticMemberContainer<QByteArray>::m_staticReferer = "";

template<class QByteArray>
int StaticMemberContainer<QByteArray>::m_staticTimeout = 10000;

template<class QByteArray>
QString StaticMemberContainer<QByteArray>::m_lastError = "";

template<class T>
QMap<QByteArray, QByteArray> StaticMemberContainer<T>::m_staticRawHeader = QMap<QByteArray, QByteArray>();

class TimeoutDebugNotifier : public QObject
{
		Q_OBJECT

	public:
		TimeoutDebugNotifier(QString msg) :
			m_msg(msg), m_triggered(false) {}

		bool Triggered()
		{
			return m_triggered;
		}

	public slots:
		void timeout()
		{
			qDebug("%s", qPrintable(m_msg));
			m_triggered = true;
		}

	private:
		QString m_msg;
		bool m_triggered;
};

#ifndef CONSOLE_ONLY
class StaticProgressUpdate : public QObject
{
		Q_OBJECT

	public:
		StaticProgressUpdate(QProgressBar *pProgress) :
			m_pProgress(pProgress) {}
		void SetMaximum(int max)
		{
			if(m_pProgress)
			{
				QMetaObject::invokeMethod(m_pProgress,
										  "setMaximum",
										  Qt::QueuedConnection,
										  Q_ARG(int, max));
			}
		}

	public slots:
		void downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
		{
			if(m_pProgress)
			{
				if(bytesTotal > 0)
					SetMaximum(bytesTotal);

				QMetaObject::invokeMethod(m_pProgress,
										  "setValue",
										  Qt::QueuedConnection,
										  Q_ARG(int, bytesReceived));
			}
		}

	private:
		QProgressBar *m_pProgress;
};
#endif

class Download : public QObject, public StaticMemberContainer<QByteArray>
{
		Q_OBJECT

	public:
#ifndef CONSOLE_ONLY
		Download(QProgressBar *pProgress = nullptr, QListWidget *pLog = nullptr)
#else
		Download()
#endif
		{
#ifndef CONSOLE_ONLY
			m_pProgress = pProgress;
			m_pLog = pLog;
#endif

			m_activeLoad = false;
			m_doTimeout = false;
			m_downloadToFile = true;
			m_loadRedirect = false;
			m_skipFlag = false;
			m_printHeaders = false;
			m_staticReferer = "";

			m_pCurrentDownload = nullptr;

			m_postData = "";

			m_linkCnt = 0;

			m_timerID = -1;
			m_timeoutMS = 10000; // 10 sec.

			m_pProxy = new QNetworkProxy();
			m_proxies = QStringList();
			m_curProxy = 0;

			m_pCookieJar = new QNetworkCookieJar();
			m_manager.setCookieJar(m_pCookieJar);

			connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply *)), Qt::UniqueConnection);

			m_dMode = GET;
		}

		~Download()
		{
			delete m_pProxy;
			delete m_pCookieJar;
		}

		/*********************
		 * Public methods
		 ********************/

		void Get(QStringList urlList, QStringList nameList = QStringList(), bool downloadToFile = false)
		{
			m_redirects.clear();
			m_rawData.clear();
			m_downloadToFile = downloadToFile;

			for(int i = 0; i < urlList.count(); i++)
			{
				m_downloadQueue.enqueue(QUrl(urlList[i]));
				if(downloadToFile)
				{
					if(!nameList.empty() && nameList.size() > i)
						m_NameQueue.enqueue(nameList[i]);
					else
						m_NameQueue.enqueue(getFileName(QUrl(urlList[i])));
				}
			}

			m_linkCnt = urlList.count();
			m_fileNames = m_NameQueue;

			m_dMode = GET;

			startLoad();
		}

		void Get(QString url, QString filename = "", bool downloadToFile = false)
		{
			m_redirects.clear();
			m_rawData.clear();
			m_downloadToFile = downloadToFile;

			m_downloadQueue.enqueue(QUrl(url));

			if(downloadToFile)
				m_NameQueue.enqueue(filename != "" ? filename : getFileName(url));

			m_dMode = GET;

			m_linkCnt = 1;
			m_fileNames = m_NameQueue;

			startLoad();
		}

		void Post(QStringList urlList, QStringList dataList, QStringList nameList = QStringList(), bool downloadToFile = false)
		{
			m_redirects.clear();
			m_rawData.clear();
			m_downloadToFile = downloadToFile;

			for(int i = 0; i < urlList.count(); i++)
			{
				m_downloadQueue.enqueue(QUrl(urlList.at(i)));
				if(dataList.size() > i)
				{
					QByteArray dat;
                    dat.append(dataList.at(i).toUtf8());
					m_postQueue.enqueue(dat);
				}
				else
					m_postQueue.enqueue(QByteArray(""));

				if(downloadToFile)
				{
					if(!nameList.empty() && nameList.size() > i)
						m_NameQueue.enqueue(nameList[i]);
					else
						m_NameQueue.enqueue(getFileName(QUrl(urlList[i])));
				}
			}

			m_linkCnt = urlList.count();
			m_fileNames = m_NameQueue;

			m_dMode = POST;

			startLoad();
		}

		void Post(QString url, QString data, QString filename = "", bool downloadToFile = false)
		{
			m_redirects.clear();
			m_rawData.clear();
			m_downloadQueue.enqueue(QUrl(url));

			QByteArray dat;
            dat.append(data.toUtf8());

			m_postQueue.enqueue(dat);

			m_downloadToFile = downloadToFile;

			if(downloadToFile)
				m_NameQueue.enqueue(filename != "" ? filename : getFileName(url));

			m_linkCnt = 1;
			m_fileNames = m_NameQueue;

			m_dMode = POST;

			startLoad();
		}

#ifndef CONSOLE_ONLY
		void SetProgressBar(QProgressBar *pProgressBar)
		{
			m_pProgress = pProgressBar;
		}

		void SetLogWidget(QListWidget *pListWidget)
		{
			m_pLog = pListWidget;
		}
#endif

		void ClearAll()
		{
			ClearHtml();

			disconnect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply *)));

			if(m_pCurrentDownload)
			{
				m_pCurrentDownload->abort();
				m_pCurrentDownload->deleteLater();
			}

			m_pCurrentDownload = nullptr;

			m_redirects.clear();
			m_postData.clear();
			m_downloadQueue.clear();
			m_NameQueue.clear();
			m_postQueue.clear();
			m_linkName.clear();
			m_linkUrl.clear();
			m_fileNames.clear();
			m_setCookies.clear();
			m_rawHeader.clear();
			m_cookies.clear();

			if(m_output.isOpen())
				m_output.close();


			connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply *)), Qt::UniqueConnection);
		}

		void ClearHtml()
		{
			m_html.clear();
			m_rawData.clear();
		}

		QString GetHtml()
		{
			m_html.clear();
			m_html.append(m_rawData);
			return m_html;
		}

		QByteArray GetRawData() const
		{
			return m_rawData;
		}

		QHash<QString, QString> GetRedirects() const
		{
			return m_redirects;
		}

		void SetProxy(QNetworkProxy proxy)
		{
			m_manager.setProxy(proxy);
		}

		void SetProxies(QStringList proxies)
		{
			m_proxies = proxies;
		}

		void EnableProxies(bool enable) const
		{
			QNetworkProxy::setApplicationProxy(enable ? *m_pProxy : QNetworkProxy());
		}

		bool NextProxy()
		{
			if(m_proxies.isEmpty())
				return false;

            QRegularExpression rx(R"(^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}:[0-9]{1,4}$)");

            while (m_proxies.at(m_curProxy).isEmpty() || !rx.match(m_proxies.at(m_curProxy)).hasMatch())
			{
				m_curProxy++;

				if(m_curProxy >= m_proxies.count() - 1)
					m_curProxy = 0;
			}

			QStringList tmp = m_proxies[m_curProxy].split(":");

			m_pProxy->setType(QNetworkProxy::HttpProxy);
			m_pProxy->setHostName(tmp.at(0));
			m_pProxy->setPort(tmp.at(1).toUInt());
			QNetworkProxy::setApplicationProxy(*m_pProxy);

			qDebug("%s:%s", qPrintable(tmp.at(0)), qPrintable(tmp.at(1)));

			m_curProxy++;

			if(m_curProxy >= m_proxies.count() - 1)
				m_curProxy = 0;

			emit nextProxy();

			return true;
		}

		void SkipProxy()
		{
			NextProxy();

			if(m_pCurrentDownload && m_pCurrentDownload->isRunning())
			{
				m_skipFlag = true;

				disconnect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply *)));

				m_pCurrentDownload->abort();

				connect(&m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(downloadFinished(QNetworkReply *)), Qt::UniqueConnection);

				m_downloadQueue.push_front(m_linkUrl);

				if(m_downloadToFile)
					m_NameQueue.push_front(m_linkName);

				if(m_dMode == POST)
					m_postQueue.push_front(m_postData);

				m_pCurrentDownload = nullptr;

				startLoad();
			}
		}

		void EnableRedirectDownload(bool enable)
		{
			m_loadRedirect = enable;
		}

		bool HasRedirects() const
		{
			return !m_redirects.isEmpty();
		}

		void EnableHeaderPrinting(bool enable)
		{
			m_printHeaders = enable;
		}

		void EnableTimeout(bool enable)
		{
			m_doTimeout = enable;
		}

		void SetTimeout(int ms)
		{
			m_timeoutMS = ms;
		}

		void AddRawHeader(QByteArray header, QByteArray value)
		{
			m_rawHeader.insert(header, value);
		}

		void ClearRawHeader()
		{
			m_rawHeader.clear();
		}

		void ClearRedirects()
		{
			m_redirects.clear();
		}

		QList<QNetworkCookie> GetCookies()
		{
			return m_cookies;
		}

		int GetLinkCnt()
		{
			return m_linkCnt;
		}

		QStringList GetFileNames()
		{
			return m_fileNames;
		}

#ifndef CONSOLE_ONLY
		static void SetStaticProgress(QProgressBar *pProgress)
		{
			m_pStaticProgress = pProgress;
		}
#endif

		static void SetStaticReferer(QString referer)
		{
			m_staticReferer.clear();
			m_staticReferer.append(referer);
		}

		static void IgnoreRedirect(bool ignore)
		{
			m_ignoreRedirect = ignore;
		}

		static void SetStaticTimeout(int timeoutMS)
		{
			m_staticTimeout = timeoutMS;
		}

		static void AddStaticRawHeader(const QByteArray& header, const QByteArray& value)
		{
			m_staticRawHeader.insert(header, value);
		}

		static QString Get_sync(QString url, QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
								QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			return QString(Download::syncDownload(url, GET, "", proxy, "", pCookies, cookieMode));
		}

		static QByteArray Get_sync_raw(QString url, QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
									   QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			return Download::syncDownload(url, GET, "", proxy, "", pCookies, cookieMode);
		}

		static QString Post_sync(QString url, QString data, QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
								 QString contentType = "", QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			return QString(Download::syncDownload(url, POST, data, proxy, contentType, pCookies, cookieMode));
		}

		static QByteArray Post_sync_raw(QString url, QString data, QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
										QString contentType = "", QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			return Download::syncDownload(url, POST, data, proxy, contentType, pCookies, cookieMode);
		}

		static QString Put_sync(QString url, QString data, QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
								QString contentType = "", QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			return QString(Download::syncDownload(url, PUT, data, proxy, contentType, pCookies, cookieMode));
		}

		static QUrl CheckUrlForRedirect(QString url)
		{
			return Download::checkUrlForRedirect(url);
		}

		static QString GetLastError()
		{
			return m_lastError;
		}

	protected:
		void timerEvent(QTimerEvent *)
		{
			qDebug("Timeout");
			SkipProxy();
		}

	private:
		QString getFileName(QUrl url) const
		{
			QString basename = QFileInfo(url.path()).fileName();

			if(basename.isEmpty())
				basename = "no_name";

			return basename;

			if(QFile::exists(basename))
			{
				qDebug("File: %s already exists, append number to end of filename.", qPrintable(basename));

				int i = 0;
				basename += '.';

				while(QFile::exists(basename + QString::number(i)))
					i++;

				basename += QString::number(i);
			}

			return basename;
		}

#if defined(DEBUG) || !defined(CONSOLE_ONLY)
		void addLogData(QString data)
#else
		void addLogData(QString)
#endif
		{
#ifdef DEBUG
			qDebug("%s", qPrintable(data));
#endif

#ifndef CONSOLE_ONLY
			if(m_pLog != nullptr)
				m_pLog->addItem(data);
#endif
		}

		void connectProgressBar(QNetworkReply *pReply, bool state)
		{
#ifndef CONSOLE_ONLY
			if(m_pProgress != nullptr)
				m_pProgress->reset();
#endif

			if(state)
				connect(pReply, SIGNAL(downloadProgress(qint64, qint64)), SLOT(downloadProgress(qint64, qint64)));
			else
				disconnect(pReply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
		}

		void allDownloadsDone()
		{
			addLogData("All Downloads Finished");
			m_activeLoad = false;
			QTimer::singleShot(0, this, SIGNAL(done()));
		}

		static QUrl checkUrlForRedirect(QUrl checkUrl)
		{
			QNetworkAccessManager manager;
			QNetworkRequest request;
			QTimer timeout;

			request.setUrl(checkUrl);
			request.setRawHeader("User-Agent", USER_AGENT);

			QNetworkReply *pReply = manager.head(request);

			QEventLoop eventLoop;

			connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
			connect(&timeout, SIGNAL(timeout()), &eventLoop, SLOT(quit()));

			timeout.start(5000);
			eventLoop.exec();

			if(pReply->error() == QNetworkReply::NoError)
			{
				QVariant possibleRedirectUrl = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
				if(possibleRedirectUrl.isValid())
					return possibleRedirectUrl.toUrl();
			}

			return checkUrl;
		}

		static QByteArray syncDownload(QString url, DOWNLOAD_MODE mode = GET, QString data = "", QNetworkProxy proxy = QNetworkProxy::DefaultProxy,
									   QString contentType = "", QList<QNetworkCookie> *pCookies = nullptr, COOKIE_MODE cookieMode = COOKIE_NON)
		{
			QEventLoop eventLoop;
			QTimer timeout;
			TimeoutDebugNotifier timeoutNotifier("Synchroniced connection reached the set timeout.");

#ifndef CONSOLE_ONLY
			StaticProgressUpdate staticProgress(m_pStaticProgress);
#endif

			QNetworkAccessManager manager;
			QNetworkCookieJar cookieJar;

			m_lastError = "";

			if(pCookies && (cookieMode & COOKIE_SET))
				cookieJar.setCookiesFromUrl(*pCookies, url);

			manager.setProxy(proxy);
			manager.setCookieJar(&cookieJar);

			connect(&manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
			connect(&timeout, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
			connect(&timeout, SIGNAL(timeout()), &timeoutNotifier, SLOT(timeout()));

			QUrl u(url);
			QByteArray dat;
			dat.append(data.toUtf8());

			QNetworkRequest request(u);
			QNetworkReply *pReply;

			request.setRawHeader("User-Agent", USER_AGENT);
			request.setRawHeader("Referer", m_staticReferer.toLatin1());

			for(const QByteArray& h : m_staticRawHeader.keys())
				request.setRawHeader(h, m_staticRawHeader.value(h));

			if(mode == GET)
				pReply = manager.get(request);
			else if(mode == POST)
			{
				request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant((contentType.isEmpty() ? "application/x-www-form-urlencoded; charset=UTF-8" : contentType)));

				pReply = manager.post(request, dat);
			}
			else if(mode == PUT)
			{
				request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant((contentType.isEmpty() ? "application/json" : contentType)));
				pReply = manager.put(request, dat);
			}
			else
				return "";

#ifndef CONSOLE_ONLY
			if(m_pStaticProgress)
			{
				staticProgress.SetMaximum(pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
				connect(pReply, SIGNAL(downloadProgress(qint64,qint64)), &staticProgress, SLOT(downloadProgress(qint64,qint64)));
			}
#endif

			if(m_staticTimeout != 0)
				timeout.start(m_staticTimeout);

			eventLoop.exec();

			if(timeout.isActive())
				timeout.stop();

			if(timeoutNotifier.Triggered())
				m_lastError = "Timeout";

			if(pReply->error() == QNetworkReply::NoError)
			{
				QVariant possibleRedirectUrl = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
				QByteArray result;

				if(possibleRedirectUrl.isValid() && !m_ignoreRedirect)
                {
                    QString redirect = possibleRedirectUrl.toString();
					if(!redirect.contains("https://") && !redirect.contains("http://"))
					{
                        QRegularExpression rx(R"(https?://[^/]+)");
                        QRegularExpressionMatch match = rx.match(url);

                        if (match.hasMatch())
                            redirect.prepend(match.captured(0));
					}

					result = syncDownload(redirect, mode, data, proxy, contentType, pCookies, cookieMode);
				}
				else
					result = pReply->readAll();

				if(pCookies && (cookieMode & COOKIE_GET))
					*pCookies = cookieJar.cookiesForUrl(QUrl(url));

				delete pReply;

				return result;
			}
			else
			{
				qDebug("Failure: %s", qPrintable(pReply->errorString()));
				m_lastError = pReply->errorString();

				QByteArray result = pReply->readAll();
				delete pReply;

				return result;
			}
		}

	private slots:
		void downloadReadyRead(QNetworkReply *pReply)
		{
			m_output.write(pReply->readAll());
		}

		void downloadFinished(QNetworkReply *pReply)
		{
			if(m_doTimeout)
				KILL_SET_TIMER(m_timerID);

			if(!m_pCurrentDownload)
			{
				pReply->deleteLater();
				return;
			}

			if(m_printHeaders)
			{
				QList<QByteArray> headerList = pReply->rawHeaderList();
				qDebug("Response header:");
				foreach(QByteArray head, headerList)
					qDebug("%s:%s", qPrintable(head), qPrintable(pReply->rawHeader(head)));
			}

			QVariant cookieVar = pReply->header(QNetworkRequest::SetCookieHeader);

			if(cookieVar.isValid())
			{
				m_setCookies.append(cookieVar.value<QList<QNetworkCookie> >());
				m_pCookieJar->setCookiesFromUrl(m_setCookies, pReply->request().url());
			}

			m_cookies = m_pCookieJar->cookiesForUrl(QUrl(m_linkUrl));

			addLogData("Download of: " + m_linkName + " finished");

			connectProgressBar(pReply, false);

			redirect(pReply);

			if(m_downloadToFile)
			{
				downloadReadyRead(pReply);
				m_output.close();
			}
			else
				readSite(pReply);

			pReply->deleteLater();
			m_pCurrentDownload = nullptr;

			startLoad();
		}

#ifdef USE_SSL
		void sslErrors(QList<QSslError> errors)
		{
			emit sslError();
		}
#endif
		void startLoad()
		{
			m_activeLoad = true;

			if(m_downloadQueue.isEmpty())
			{
				allDownloadsDone();
				return;
			}

			m_linkUrl = m_downloadQueue.dequeue();
			m_linkName = m_downloadToFile ? m_NameQueue.dequeue() : "";

			addLogData("Start download of: " + m_linkUrl.toEncoded());

			if(m_downloadToFile)
			{
				m_output.setFileName(m_linkName);

				if(!m_output.open(QIODevice::WriteOnly))
				{
					addLogData("Error while opening the Output-File for: " + m_linkName);

					startLoad();
					return;
				}
			}

			QNetworkRequest request(m_linkUrl);

			if(!m_setCookies.isEmpty())
				request.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(m_setCookies));

			if(!m_rawHeader.isEmpty())
			{
				foreach(QByteArray header, m_rawHeader.keys())
					request.setRawHeader(header, m_rawHeader.value(header));
			}

			if(m_dMode == GET)
				m_pCurrentDownload = m_manager.get(request);
			else if(m_dMode == POST)
			{
				m_postData = m_postQueue.dequeue();

				request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/x-www-form-urlencoded; charset=UTF-8"));
				request.setHeader(QNetworkRequest::ContentLengthHeader, QVariant(m_postData.size()));
				request.setRawHeader("User-Agent", USER_AGENT);
				request.setRawHeader("Referer", m_staticReferer.toLatin1());

				m_pCurrentDownload = m_manager.post(request, m_postData);
			}
			else if(m_dMode == HEAD)
				m_pCurrentDownload = m_manager.head(request);

			connectProgressBar(m_pCurrentDownload, true);

			if(m_doTimeout)
			{
				KILL_SET_TIMER(m_timerID);
				m_timerID = startTimer(m_timeoutMS);
			}

#ifdef USE_SSL
			connect(m_pCurrentDownload, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrors(QList<QSslError>)));
#endif
		}

		void readSite(QNetworkReply *pReply)
		{
			if(pReply->isReadable())
				m_rawData.append(pReply->readAll());
		}

		void redirect(QNetworkReply *pReply)
		{
			QVariant possibleRedirectUrl = pReply->attribute(QNetworkRequest::RedirectionTargetAttribute);

			if(!possibleRedirectUrl.toUrl().toString().isEmpty())
				m_redirects.insert(m_linkUrl.toString(), possibleRedirectUrl.toString());
			else
				return;

			if(m_loadRedirect)
			{
				ClearHtml();
				m_dMode = GET;
				m_downloadQueue.push_front(possibleRedirectUrl.toUrl().toString());

				if(m_downloadToFile)
					m_NameQueue.push_front(m_linkName);

				if(m_dMode == POST)
					m_postQueue.push_front(m_postData);
			}
		}

#ifdef CONSOLE_ONLY
		void downloadProgress(qint64, qint64)
#else
		void downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
#endif
		{
#ifndef CONSOLE_ONLY
			if(m_pProgress != nullptr)
			{
				m_pProgress->setMaximum(bytesTotal);
				m_pProgress->setValue(bytesReceived);
			}
#endif

			if(m_doTimeout)
			{
				KILL_SET_TIMER(m_timerID);
				m_timerID = startTimer(m_timeoutMS);
			}
		}

	signals:
		void done();
		void nextProxy();
#ifdef USE_SSL
		void sslError();
#endif

	private:
		QNetworkAccessManager m_manager;
		QNetworkReply *m_pCurrentDownload;
		QNetworkCookieJar *m_pCookieJar;

		QHash<QString, QString>	m_redirects;
		QQueue<QString> m_NameQueue;

#ifndef CONSOLE_ONLY
		QProgressBar *m_pProgress;
		QListWidget  *m_pLog;
#endif

		QByteArray	 m_postData;
		QQueue<QUrl> m_downloadQueue;
		QQueue<QByteArray> m_postQueue;

		QByteArray m_rawData;
		QString	m_linkName;
		QString	m_html;
		QUrl	m_linkUrl;
		QFile	m_output;
		bool	m_activeLoad;
		bool    m_doTimeout;
		bool 	m_downloadToFile;
		bool    m_loadRedirect;
		bool	m_skipFlag;
		bool    m_printHeaders;
		int     m_timerID;
		int     m_timeoutMS;

		int m_linkCnt;
		QStringList m_fileNames;

		DOWNLOAD_MODE m_dMode;

		QList<QNetworkCookie> m_setCookies;

		QHash<QByteArray, QByteArray> m_rawHeader;
		QList<QNetworkCookie> m_cookies;

		//	Proxy stuff
		QStringList m_proxies;
		QNetworkProxy *m_pProxy;
		int m_curProxy;
};

#endif // DOWNLOAD_H
