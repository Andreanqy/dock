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
    List<Control^>^ onboard;
    int capacitySlots;
    int usedSlots;
    ParomState currentState;
    bool visible_;

    // ДЛЯ АСИНХРОННОЙ ЗАГРУЗКИ
    Timer^ loadingTimer;
    List<Car^>^ loadingQueue;
    // Timer^ unloadTimer;

    // Для анимации заезда
    Timer^ carMovementTimer;
    Car^ currentMovingCar;
    Point targetPositionOnFerry;

public:
    bool isLeftSide;

    // События
    event SimpleEventHandler^ OnUnloadStart;
    event SimpleEventHandler^ OnLoadingStarted;
    event SimpleEventHandler^ OnLoadingFinished; 
    event SimpleEventHandler^ OnUnloadingFinished;
    event ParomStateEventHandler^ OnParomStateChanged;

    // Новое событие для анимации загрузки
    event EventHandler^ OnCarLoadingProgress;

public:
    Parom(PictureBox^ spriteParom, int capacityInSlots) : sprite(spriteParom), onboard(gcnew List<Control^>()), capacitySlots(capacityInSlots), usedSlots(0), currentState(ParomState::Waiting), visible_(true), isLeftSide(true), loadingQueue(gcnew List<Car^>()), currentMovingCar(nullptr)
    {
        if (sprite == nullptr) throw gcnew ArgumentNullException("spriteParom");
        sprite->Visible = true;

        // Инициализация таймера загрузки
        loadingTimer = gcnew Timer();
        loadingTimer->Interval = 100; // 100мс между машинами
        loadingTimer->Tick += gcnew EventHandler(this, &Parom::OnLoadingTick);

        // Инициализация таймера анимации машин
        carMovementTimer = gcnew Timer();
        carMovementTimer->Interval = 50; // 50мс для плавной анимации
        carMovementTimer->Tick += gcnew EventHandler(this, &Parom::OnCarMovementTick);
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

    List<Control^>^ Onboard() { return onboard; }

    // Начать асинхронную загрузку
    void StartAsyncLoading(List<Car^>^ carsToLoad)
    {
        if (currentState != ParomState::Waiting) {
            return;
        }

        currentState = ParomState::Loading;
        loadingQueue = carsToLoad;

        OnLoadingStarted();
        OnParomStateChanged(this);
        loadingTimer->Start();
    }

    // Обработчик тика загрузки
    void OnLoadingTick(Object^ sender, EventArgs^ e)
    {
        // Если уже кто-то заезжает – ждём завершения
        if (currentMovingCar != nullptr)
            return;

        // Требование: начинать загрузку только когда есть минимум 2 машины в очереди,
        // если паром сейчас пуст (usedSlots == 0)
        //if (usedSlots == 0 && loadingQueue != nullptr && loadingQueue->Count < 2)
        //    return;

        // Разрешаем старт с одной машины, если очередь пуста, или если в ней грузовик
        /*
        if (usedSlots == 0 && loadingQueue != nullptr) {
            bool hasTruck = false;
            for each (Car ^ c in loadingQueue)
                if (dynamic_cast<Truck^>(c) != nullptr)
                    hasTruck = true;

            if (!hasTruck && loadingQueue->Count < 2)
                return;
        }
        */

        if (loadingQueue->Count == 0 || IsFull)
        {
            CheckLoadingCompletion();
            return;
        }

        Car^ currentCar = loadingQueue[0];

        // Запускаем анимацию въезда (без мгновенного добавления на борт)
        StartCarLoadingAnimation(currentCar);

        // Удаляем из очереди — теперь эта машина считается в процессе загрузки
        loadingQueue->RemoveAt(0);

        // Следующую возьмём после завершения анимации текущей
        CheckLoadingCompletion();
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
            OnLoadingFinished();
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

    // Начать выгрузку
    void StartUnloading()
    {
        currentState = ParomState::Unloading;
        OnUnloadStart();
        OnParomStateChanged(this);
    }

    // Завершение выгрузки
    void FinishUnloading()
    {
        usedSlots = 0;
        onboard->Clear();
        currentState = ParomState::Waiting;
        OnUnloadingFinished();
        OnParomStateChanged(this);
    }

    // Проверка возможности загрузки с берега
    bool CanLoadFromShore(bool shoreIsLeft)
    {
        return (currentState == ParomState::Waiting && isLeftSide == shoreIsLeft && usedSlots == 0);
    }

    void StartCarLoadingAnimation(Car^ car)
    {
        if (car == nullptr || car->Sprite == nullptr) return;

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
            // переносим внутрь парома
            Point local(targetPositionOnFerry.X - sprite->Left, targetPositionOnFerry.Y - sprite->Top);
            //currentMovingCar->Sprite->Parent = sprite;
            currentMovingCar->Sprite->Location = local;

            // машина на палубе не видна (паром визуально без изменений)
            currentMovingCar->Sprite->Visible = false;

            // учитываем на борту ТЕПЕРЬ, после фактического въезда
            onboard->Add(currentMovingCar->Sprite);
            usedSlots += currentMovingCar->Slots;

            // останавливаем шаги въезда
            carMovementTimer->Stop();

            OnCarLoadingProgress(this, System::EventArgs::Empty);

            currentMovingCar = nullptr;
        }
    }
};

// Частичная специализация под автомобили
public ref class CarsParom : public Parom
{
public:
    CarsParom(PictureBox^ spriteParom, int capacityInSlots) : Parom(spriteParom, capacityInSlots) { }
};