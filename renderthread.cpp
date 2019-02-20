#include "renderthread.h"

RenderThread::RenderThread(AbstractFunction *function, ColorWheel *colorwheel, Settings *settings, QSize outputSize, QObject *parent) : QThread(parent)
{
    restart = false;
    abort = false;
    
    currFunction = function;
    currColorWheel = colorwheel;
    currSettings = settings;
    
    overallWidth = outputSize.width();
    overallHeight = outputSize.height();
    
    controllerCondition = new QWaitCondition();
}

RenderThread::~RenderThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    
    wait();
}

void RenderThread::render(QPoint topLeft, QPoint bottomRight, QWaitCondition *controllerCondition)
{
    QMutexLocker locker(&mutex);
    
    this->topLeft = topLeft;
    this->bottomRight = bottomRight;
    topLeftXValue = topLeft.x();
    topLeftYValue = topLeft.y();
    
    bottomRightXValue = bottomRight.x();
    bottomRightYValue = bottomRight.y();
    
    this->controllerCondition = controllerCondition;
    worldYStart1 = currSettings->Height + currSettings->YCorner;
    worldYStart2 = currSettings->Height/overallHeight;
    worldXStart = currSettings->Width/overallWidth;
    
    if (!isRunning()) {
        start(InheritPriority);
    } else {
        restart = true;
        condition.wakeOne();
    }
}


void RenderThread::run()
{
    forever {
        mutex.lock();
        
        double worldX, worldY,ang;
        double worldYStart1 = this->worldYStart1;
        double worldYStart2 = this->worldYStart2;
        double worldXStart = this->worldXStart;
        double XCorner = currSettings->XCorner;
        
        
        double outputWidth = bottomRightXValue - topLeftXValue;
        double outputHeight = bottomRightYValue - topLeftYValue;
        
        int translated = bottomRightXValue;
        std::complex<double> fout;
        QPoint topLeft = this->topLeft;
        QVector<QVector<QRgb>> colorMap(outputWidth, QVector<QRgb>(outputHeight));
        
        mutex.unlock();
        
        
        
        for (int x = 0; x < outputWidth; x++)
        {
            if (restart) { /* qDebug() << "renderThread aborts" ; */ break; }
            if (abort) return;
            
            for (int y = 0; y < outputHeight; y++)
            {
                if (restart) break;
                if (abort) return;
                
                worldX = (x + translated) * worldXStart + XCorner;
                worldY = worldYStart1 - y * worldYStart2;
                
                //run the point through our mathematical function
                //...then convert that complex output to a color according to our color wheel
                ang=qAtan2(worldY,worldX)-2*pi*(currFunction->getF1()/ currFunction->getF2())*currFunction->getT();
                fout = (*currFunction)(qLn(qSqrt(worldX*worldX+worldY*worldY)),ang);  // math function to get complex coordinate
                QRgb color = (*currColorWheel)(fout);   // colorification from complex coordinate

                if (y % 10 == 0 && x % 10 == 0) {
                    emit newImageDataPoint(fout);
                }
                
                // now, push the determined color to the corresponding point on the display
                colorMap[x][y] = color;
            }
            
            if (x % 100 == 0) {
                emit newProgress((x/outputWidth) * 100);
            }
        }

        // qDebug() << currentThreadId() << "FINISHES RENDERING";
        
        mutex.lock();
        
        if (!restart) {
            emit renderingFinished(topLeft, colorMap);

            condition.wait(&mutex);
        }
        // qDebug() << "thread" << QThread::currentThread() << "wakes up from restarting";
        restart = false;
        mutex.unlock();        
    }   
}
