#pragma once
#include "Delegates.h"  // Добавляем в начале
#include "human.h"      // Добавляем для SecurityGuard
#include "Car.h"  // Добавляем для работы с Car^

using namespace System;
using namespace System::Drawing;
using namespace System::Windows::Forms;
using namespace System::Collections::Generic;

public ref class Parom
{
public:
    enum class ParomState { Waiting, Loading, MovingToDest, Unloading };

private:
    PictureBox^ sprite;
    int capacitySlots;
    int usedSlots;
    ParomState currentState;
    bool visible_;

    // ДЛЯ АСИНХРОННОЙ ЗАГРУЗКИ
    Timer^ loadingTimer;
    List<Car^>^ loadingQueue;

    // ДЛЯ АСИНХРОННОЙ РАЗГРУЗКИ
    Timer^ unloadingTimer;
    List<Car^>^ unloadingQueue;

    // Для анимации заезда
    Timer^ carMovementTimer;

    // Для анимации выезда
    Timer^ carMovementBackTimer;

    Car^ currentMovingCar;
    Point targetPositionOnFerry;
    Point targetPositionOnShore;

public:
    bool isLeftSide;

    // События
    event ParomStateEventHandler^ OnParomStateChanged;

    // Новое событие для анимации загрузки
    event EventHandler^ OnCarLoadingProgress;

public:
    Parom(PictureBox^ spriteParom, int capacityInSlots) : sprite(spriteParom), capacitySlots(capacityInSlots), usedSlots(0), currentState(ParomState::Waiting), visible_(true), isLeftSide(true), loadingQueue(gcnew List<Car^>()), currentMovingCar(nullptr)
    {
        capacityInSlots = 3;
        unloadingQueue = gcnew List<Car^>();

        if (sprite == nullptr) throw gcnew ArgumentNullException("spriteParom");
        sprite->Visible = true;

        // Инициализация таймера загрузки
        loadingTimer = gcnew Timer();
        loadingTimer->Interval = 100; // 100мс между машинами
        loadingTimer->Tick += gcnew EventHandler(this, &Parom::OnLoadingTick);

        // Инициализация таймера разгрузки
        unloadingTimer = gcnew Timer();
        unloadingTimer->Interval = 100; // 100мс между машинами
        unloadingTimer->Tick += gcnew EventHandler(this, &Parom::OnUnloadingTick);

        // Инициализация таймера анимации машин
        carMovementTimer = gcnew Timer();
        carMovementTimer->Interval = 50; // 50мс для плавной анимации
        carMovementTimer->Tick += gcnew EventHandler(this, &Parom::OnCarMovementTick);

        // Инициализация таймера анимации машин (при разгрузке)
        carMovementBackTimer = gcnew Timer();
        carMovementBackTimer->Interval = 50; // 50мс для плавной анимации
        carMovementBackTimer->Tick += gcnew EventHandler(this, &Parom::OnCarMovementBackTick);
    }

    property PictureBox^ Sprite { PictureBox^ get() { return sprite; } }
    property ParomState State {
        ParomState get() { return currentState; }
        void set(ParomState value) {
            currentState = value;
        }
    }
    property int CapacitySlots { int get() { return capacitySlots; } }
    property int UsedSlots { int get() { return usedSlots; } }
    property int FilledSlots { int get() { return usedSlots; } }
    property bool IsVisible { bool get() { return visible_; } void set(bool v) { visible_ = v; if (sprite) sprite->Visible = v; } }
    property bool IsEmpty { bool get() { return usedSlots == 0; } }
    property bool IsFull { bool get() { return usedSlots >= capacitySlots; } }

    // Проверка возможности загрузки с берега
    bool CanLoadFromShore(bool shoreIsLeft)
    {
        return (currentState == ParomState::Waiting && isLeftSide == shoreIsLeft && usedSlots == 0);
    }



    // ===========
    // ЗАГРУЗКА
    // ===========

    // Начать асинхронную загрузку
    void StartAsyncLoading(List<Car^>^ carsToLoad)
    {
        if (currentState != ParomState::Waiting) {
            return;
        }

        currentState = ParomState::Loading;
        loadingQueue = carsToLoad;

        loadingTimer->Start();
    }

    // Обработчик тика загрузки
    void OnLoadingTick(Object^ sender, EventArgs^ e)
    {
        // Если уже кто-то заезжает – ждём завершения
        if (currentMovingCar != nullptr) return;

        if (loadingQueue->Count == 0 || IsFull)
        {
            CheckLoadingCompletion();
            return;
        }

        Car^ currentCar = loadingQueue[0];

        // Запускаем анимацию въезда (без мгновенного добавления на борт)
        StartCarLoadingAnimation(currentCar);

        // Удаляем из очереди — теперь эта машина считается в процессе загрузки
        unloadingQueue->Add(loadingQueue[0]);
        loadingQueue->RemoveAt(0);

        // Следующую возьмём после завершения анимации текущей
        CheckLoadingCompletion();
    }

    void StartCarLoadingAnimation(Car^ car)
    {
        currentMovingCar = car;

        // Паром сверху; машина под ним (чтобы "ныряла" визуально)
        sprite->BringToFront();
        car->Sprite->SendToBack();

        int targetX = (sprite->Left) + (isLeftSide ? 10 : sprite->Width - car->Sprite->Width - 10);
        int targetY = sprite->Top + sprite->Height / 2 - car->Sprite->Height / 2;

        targetPositionOnFerry = Point(targetX, targetY);

        // Запускаем шаговую анимацию въезда
        carMovementTimer->Start();
    }

    void OnCarMovementTick(Object^ sender, EventArgs^ e)
    {
        if (currentMovingCar == nullptr) {
            carMovementTimer->Stop();
            return;
        }

        // Плавно двигаем машину к позиции на пароме
        bool reachedX = currentMovingCar->MoveToX(targetPositionOnFerry.X);
        bool reachedY = currentMovingCar->MoveToY(targetPositionOnFerry.Y);

        if (reachedX && reachedY) {
            // переносим к месту парома для будущей разгрузки
            Point local(isLeftSide ? 600 : 465, isLeftSide ? 190 : 450); // 600 : 0 и 190 : 0
            //currentMovingCar->Sprite->Parent = sprite;
            currentMovingCar->Sprite->Location = local;

            // машина на палубе не видна (паром визуально без изменений)
            currentMovingCar->Sprite->Visible = false;

            // учитываем на борту ТЕПЕРЬ, после фактического въезда
            usedSlots += currentMovingCar->Slots;

            // останавливаем шаги въезда
            carMovementTimer->Stop();

            OnCarLoadingProgress(this, System::EventArgs::Empty);

            currentMovingCar = nullptr;
        }
    }
    
    // Проверка условий завершения загрузки
    void CheckLoadingCompletion()
    {
        // Если палуба полная — завершаем загрузку и отплываем
        if (IsFull)
        {
            Console::WriteLine("Паром полностью загружен, отплываем!");
            State = ParomState::MovingToDest;
            loadingQueue->Clear();
            OnParomStateChanged(this);
            return;
        }

        // Если очередь пустая — просто ждём
        if (loadingQueue == nullptr || loadingQueue->Count == 0)
        {
            Console::WriteLine("Очередь пуста, ждем пополнения.");
            return;
        }

        // Если очередь есть, продолжаем загружать
        if (currentMovingCar == nullptr)
        {
            // берём следующую
            OnLoadingTick(nullptr, nullptr);
        }
    }



    // ===========
    // РАЗГРУЗКА
    // ===========

    // Начать асинхронную разгрузку
    void StartAsyncUnloading()
    {
        if (currentState != ParomState::MovingToDest) {
            return;
        }

        currentState = ParomState::Unloading;

        unloadingTimer->Start();
    }

    // Обработчик тика разгрузки
    void OnUnloadingTick(Object^ sender, EventArgs^ e)
    {
        // Если уже кто-то выезжает – ждём завершения
        if (currentMovingCar != nullptr) return;

        if (unloadingQueue->Count == 0 || IsEmpty)
        {
            CheckUnloadingCompletion();
            return;
        }

        Car^ currentCar = unloadingQueue[0];
        currentCar->Sprite->Visible = true;

        // Запускаем анимацию выезда (без мгновенного добавления на берег)
        StartCarUnloadingAnimation(currentCar);

        // Удаляем из очереди — теперь эта машина считается в процессе разгрузки
        unloadingQueue->RemoveAt(0);

        // Следующую возьмём после завершения анимации текущей
        CheckUnloadingCompletion();
    }

    void StartCarUnloadingAnimation(Car^ car)
    {
        currentMovingCar = car;

        int targetX = isLeftSide ? 1300 : -200;
        int targetY = isLeftSide ? 190 : 450;

        targetPositionOnShore = Point(targetX, targetY);

        // Запускаем пошаговую анимацию въезда
        carMovementBackTimer->Start();
    }

    void OnCarMovementBackTick(Object^ sender, EventArgs^ e)
    {
        if (currentMovingCar == nullptr)
        {
            carMovementBackTimer->Stop();
            return;
        }

        // Плавно двигаем машину к позиции на пароме
        bool reachedX = currentMovingCar->MoveToX(targetPositionOnShore.X);
        bool reachedY = currentMovingCar->MoveToY(targetPositionOnShore.Y);

        if (reachedX && reachedY)
        {
            currentMovingCar->Sprite->Visible = true;
            usedSlots -= currentMovingCar->Slots;

            // останавливаем шаги въезда
            carMovementBackTimer->Stop();

            OnCarLoadingProgress(this, System::EventArgs::Empty);

            currentMovingCar = nullptr;
        }
    }

    // Проверка условий завершения разгрузки
    void CheckUnloadingCompletion()
    {
        // Если палуба пустая — завершаем разгрузку и ожидаем загрузку
        if (IsEmpty)
        {
            Console::WriteLine("Паром полностью пуст, можно загружать!");
            State = ParomState::Waiting;
            unloadingQueue->Clear();
            isLeftSide = !isLeftSide; // инверсия стороны после выгрузки
            int a = 0;
            // Вызвать CheckQueues
            unloadingTimer->Stop();
            return;
        }

        // Если очередь есть, продолжаем разгружать
        else
        {
            // берём следующую
            OnUnloadingTick(nullptr, nullptr);
        }
    }
};


    
// Частичная специализация под автомобили
public ref class CarsParom : public Parom
{
public:
    CarsParom(PictureBox^ spriteParom, int capacityInSlots) : Parom(spriteParom, capacityInSlots) { }
};