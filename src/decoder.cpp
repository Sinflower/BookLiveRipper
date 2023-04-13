#include "decoder.h"

#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QDir>

int Decoder::m_arr[128] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

Decoder::Decoder(QString bookID, QString decodeKey, QString ptbl, QString ctbl, QString stbl, QString ttbl)
{
	calcDecodeCode(bookID, decodeKey);

	m_ptbl = decodeData(ptbl);
	m_ctbl = decodeData(ctbl);
	m_stbl = decodeData(stbl);
	m_ttbl = decodeData(ttbl);

	Q_ASSERT(!m_ptbl.isEmpty());
	Q_ASSERT(!m_ctbl.isEmpty());
	Q_ASSERT(!m_stbl.isEmpty());
	Q_ASSERT(!m_ttbl.isEmpty());
}

// TODO: Make this look normal ...
//       Better variable names?
bool Decoder::DecodeImage(QString inName, QByteArray imgData, QString outName)
{
	if(inName.isEmpty())
	{
		qDebug("[%s]: No input file specified", __func__);
		return false;
	}

	if(outName.isEmpty())
	{
		qDebug("[%s]: No output file specified", __func__);
		return false;
	}

	if(!mkPath(outName))
	{
		qDebug("[%s]: An error occured while creating the output folder for: %s", __func__, qPrintable(outName));
		return false;
	}

	// --------------- Descramble keys ---------------
	int c = 0;
	int p = 0;
	int d = inName.lastIndexOf("/") + 1;
	int e = inName.length() - d;

	if(e > 0)
	{
		for (int i = 0; i < e; i++)
		{
			int val = inName.at(i + d).toLatin1();
			if(i % 2)
				c += val;
			else
				p += val;
		}

		p = p % 8; // p2
		c = c % 8; // p1
	}
	// --------------- Descramble keys ---------------

	// --------------- Get decode data ---------------
	QRegExp rx("^=([0-9]+)-([0-9]+)([-+])([0-9]+)-([-_0-9A-Za-z]+)$");
	rx.indexIn(m_ctbl.at(c).toString());
	QStringList a = rx.capturedTexts();
	rx.indexIn(m_ptbl.at(p).toString());
	QStringList b = rx.capturedTexts();

	if (a.at(1) != b.at(1) || a.at(2) != b.at(2) || a.at(4) != b.at(4) || a.at(3) != "+" || b.at(3) != "-")
	{
		qDebug("[%s]: Invalid decode data - possible error during data decoding", __func__);
		return false;
	}

	int h = a.at(2).toInt();
	int v = a.at(2).toInt();
	int padding = a.at(4).toInt();

	if(h > 8 || v > 8 || h * v > 64)
	{
		qDebug("[%s]: Invalid h or v value: h: %d v: %d", __func__, h, v);
		return false;
	}

	e = h + v + h * v;

	if(a.at(5).length() != e || b.at(5).length() != e)
	{
		qDebug("[%s]: Calculated and actual data length mismatch", __func__);
		return false;
	}
	// --------------- Get decode data ---------------

	// --------------- Some data setup ---------------
	QList< QList<int> > s_ll = calcTNP(a.at(5), h, v);
	QList< QList<int> > d_ll = calcTNP(b.at(5), h, v);

	QList<int> st = s_ll.at(0);
	QList<int> dt = d_ll.at(0);

	QList<int> sn = s_ll.at(1);
	QList<int> dn = d_ll.at(1);

	QList<int> p_;

	for(int i = 0; i < h * v; i++)
		p_.append(s_ll.at(2).at(d_ll.at(2).at(i)));
	// --------------- Some data setup ---------------

	// --------------- Descramble image --------------
	// Yay for scopes and variable shadowing ...
	// I really need to rename this shit
	{
		QImage srcImg = QImage::fromData(imgData);
		if(srcImg.isNull())
		{
			qDebug("[%s]: Unable to load image from memory", __func__);
			return false;
		}

		int w_ = srcImg.width() - h * 2 * padding;
		int h_ = srcImg.height() - v * 2 * padding;
		int c = (w_ +  h - 1) / h;
		int e =  w_ - (h - 1) * c;
		int f = (h_ +  v - 1) / v;
		int g =  h_ - (v - 1) * f;
		int dpx, dpy, dx, dy, spx, spy, sx, sy, pw, ph;

		if(srcImg.width() < 64 || srcImg.height() < 64 || srcImg.width() * srcImg.height() < 320 * 320)
		{
			if(!srcImg.save(outName, nullptr, 100))
			{
				qDebug("[%s]: Unable to save result image to: %s", __func__, qPrintable(outName));
				return false;
			}

			return true;
		}

		// The real picture is slightly (h * v) smaller than the input picture
		QImage dstImg(w_, h_, srcImg.format());
		dstImg.fill(0);

		QPainter painter(&dstImg);

		for(int k = 0; k < h * v; k++)
		{
			dpx = k % h;
			dpy = k / h;
			dx = padding + dpx * (c + 2 * padding) + ((dn.at(dpy) < dpx) ? e - c : 0);
			dy = padding + dpy * (f + 2 * padding) + ((dt.at(dpx) < dpy) ? g - f : 0);
			spx = p_.at(k) % h;
			spy = p_.at(k) / h;
			sx = spx * c + ((sn.at(spy) < spx) ? e - c : 0);
			sy = spy * f + ((st.at(spx) < spy) ? g - f : 0);
			pw = (dn.at(dpy) == dpx ? e : c);
			ph = (dt.at(dpx) == dpy ? g : f);

			if (w_ > 0 && h_ > 0)
			{
				// TODO: Comment on s/d xy order
				painter.drawImage(sx, sy, srcImg, dx, dy, pw, ph);
			}
		}

		painter.end();

		if(!dstImg.save(outName, nullptr, 100))
		{
			qDebug("[%s]: Unable to save result image to: %s", __func__, qPrintable(outName));
			return false;
		}

		return true;
	}
	// --------------- Descramble image --------------
}

void Decoder::DecodeB64Image(QByteArray imgData, QString outName)
{
	if(!mkPath(outName))
	{
		qDebug("[%s]: An error occured while creating the output folder for: %s", __func__, qPrintable(outName));
		return;
	}

	QFile f(outName);

	if(!f.open(QIODevice::WriteOnly))
	{
		qDebug("[%s]: Unable to open output file: %s", __func__, qPrintable(f.fileName()));
		exit(-1);
	}

	// Because the string starts with "image/png;base64,"
	imgData.remove(0, imgData.indexOf(",") + 1);

	f.write(QByteArray::fromBase64(imgData));
	f.close();
}

// TODO: Add < 0x100 and > 0x10000 stuff from the js code
QString Decoder::DecodeCharacter(QString str)
{
	int x = str.toInt(nullptr, 36);
	int d = 64;
	int r = x % d;
	int s = m_stbl.at(r).toInt();
	int t = m_ttbl.at(r).toInt();
	int c = ((x - r) / d - t) / s;

	return QString("&#x%1;").arg(QString::number(c, 16));
}

/*
 * Private
 */

void Decoder::calcDecodeCode(QString bookID, QString userKey)
{
	QString str = QString("%1:%2").arg(bookID).arg(userKey);
	long b = 0;

	for(int i = 0; i < str.length(); i++)
		b += (str.at(i).toLatin1() << (i % 16));

	b &= 0x7FFFFFFF;
	if (b == 0)
		b = 0x12345678;

	m_decryptCode = b;
}

QJsonArray Decoder::decodeData(QString data)
{
	long e = m_decryptCode;
	QString decodedData = "";

	for(int i = 0; i < data.length(); i++)
	{
		e = (e >> 1) ^ (-(e & 1) & 0x48200004);
		int c = data.at(i).toLatin1() - 0x20;
		int n = ((c + e) % 0x5E) + 0x20;
		decodedData.append(QChar(n));
	}

	QJsonDocument json = QJsonDocument::fromJson(decodedData.toLatin1());

	return json.array();
}

QList< QList<int> > Decoder::calcTNP(QString str, int h, int v)
{
	QList<int> t;
	QList<int> n;
	QList<int> p;

	for(int i = 0; i < h; i++)
		t.append(m_arr[str.at(i).toLatin1()]);

	for(int i = 0; i < v; i++)
		n.append(m_arr[str.at(h + i).toLatin1()]);

	for(int i = 0; i < h * v; i++)
		p.append(m_arr[str.at(h + v + i).toLatin1()]);

	QList< QList<int> > ret;
	ret.append(t);
	ret.append(n);
	ret.append(p);

	return ret;
}

bool Decoder::mkPath(QString name)
{
	QFileInfo d(name);
	QDir dir = QDir::current();

	if(!dir.exists(d.dir().path()))
		dir.mkpath(d.dir().path());

	return dir.exists(d.dir().path());
}
