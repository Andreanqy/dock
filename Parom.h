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
    int currentLoadingIndex;
    Timer^ unloadTimer;

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
    Parom(PictureBox^ spriteParom, int capacityInSlots)
        : sprite(spriteParom),
        onboard(gcnew List<Control^>()),
        capacitySlots(capacityInSlots),
        usedSlots(0),
        currentState(ParomState::Waiting),
        visible_(true),
        isLeftSide(true),
        loadingQueue(gcnew List<Car^>()),
        currentLoadingIndex(0),
        currentMovingCar(nullptr)  // Добавляем инициализацию
    {
        if (sprite == nullptr) throw gcnew ArgumentNullException("spriteParom");
        sprite->Visible = true;

        // Инициализация таймера загрузки
        loadingTimer = gcnew Timer();
        loadingTimer->Interval = 100; // 100мс между машинами
        loadingTimer->Tick += gcnew EventHandler(this, &Parom::OnLoadingTick);

        // ДОБАВЛЯЕМ ЗДЕСЬ: инициализация таймера анимации машин
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

    void Reset()
    {
        currentState = ParomState::Waiting;
        usedSlots = 0;
        onboard->Clear();
        loadingTimer->Stop();
        loadingQueue->Clear();
        currentLoadingIndex = 0;
    }

    List<Control^>^ Onboard() { return onboard; }

    // НОВЫЙ МЕТОД: Начать асинхронную загрузку
    void StartAsyncLoading(List<Car^>^ carsToLoad)
    {
        if (currentState != ParomState::Waiting) {
            return;
        }

        currentState = ParomState::Loading;
        loadingQueue = carsToLoad;
        currentLoadingIndex = 0;

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

        if (currentLoadingIndex >= loadingQueue->Count || IsFull) {
            CheckLoadingCompletion();
            return;
        }

        Car^ currentCar = loadingQueue[currentLoadingIndex];

        // Запускаем анимацию въезда (без мгновенного добавления на борт)
        StartCarLoadingAnimation(currentCar);

        // Удаляем из очереди — теперь эта машина считается в процессе загрузки
        loadingQueue->RemoveAt(currentLoadingIndex);

        // Следующую возьмём после завершения анимации текущей
        CheckLoadingCompletion();
    }

    // Проверка позиции машины для загрузки
    bool IsCarAtLoadingPosition(Car^ car)
    {
        if (car == nullptr || car->Sprite == nullptr) return false;

        // ВРЕМЕННО: используем приблизительные координаты
        // Позже передадим реальные координаты из MyForm
        int targetX = isLeftSide ? 700 : 1800; // Примерные координаты
        return Math::Abs(car->Sprite->Left - targetX) <= 5;
    }

    // Проверка условий завершения загрузки
    void CheckLoadingCompletion()
    {
        // Если палуба полная — можно отплывать
        if (IsFull)
        {
            Console::WriteLine("Паром полностью загружен, отплываем!");
            State = ParomState::MovingToDest;
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
            currentLoadingIndex = 0; // берём следующую
            OnLoadingTick(nullptr, nullptr);
        }

        /*
        bool shouldFinish = false;

        if (IsFull) {
            shouldFinish = true;
        }
        else if (usedSlots >= 2 && (currentLoadingIndex >= loadingQueue->Count || loadingQueue->Count == 0)) {
            shouldFinish = true;
        }

        if (shouldFinish) {
            loadingTimer->Stop();
            FinishLoading();
        }
        */
    }

    // Завершение загрузки
    void FinishLoading()
    {
        currentState = ParomState::MovingToDest;
        loadingQueue->Clear();
        currentLoadingIndex = 0;

        OnLoadingFinished();
        OnParomStateChanged(this);
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
        return (currentState == ParomState::Waiting &&
            isLeftSide == shoreIsLeft &&
            usedSlots == 0);
    }

    // Загрузка машины (с анимацией)
    bool TryLoadCar(Control^ carSprite, int carSlots)
    {
        // ВРЕМЕННО: ВСЕГДА загружаем успешно

        onboard->Add(carSprite);
        usedSlots += carSlots;

        return true;
    }

    bool TryLoadCar(Control^ carSprite, int carSlots, int spacingPx)
    {
        if (carSprite == nullptr) return false;
        if (carSlots <= 0) carSlots = 1;
        if (usedSlots + carSlots > capacitySlots) return false;

        // Позиция на пароме для новой машины
        int offsetX = 10;
        for each (Control ^ c in onboard)
            offsetX += c->Width + spacingPx;

        // Устанавливаем позицию на пароме
        carSprite->Parent = sprite;
        carSprite->Top = sprite->Height / 2 - carSprite->Height / 2;
        carSprite->Left = offsetX;

        onboard->Add(carSprite);
        usedSlots += carSlots;

        return true;
    }

    void StartCarLoadingAnimation(Car^ car)
    {
        if (car == nullptr || car->Sprite == nullptr) return;

        currentMovingCar = car;

        // Паром сверху; машина под ним (чтобы "ныряла" визуально)
        sprite->BringToFront();
        car->Sprite->SendToBack();

        // Рассчитываем следующую позицию внутри парома
        //int offsetX = 10;
        //for each (Control ^ c in onboard)
        //    offsetX += c->Width + 5;

        int offsetY = sprite->Height / 2 - car->Sprite->Height / 2;

        // ВАЖНО: цель движения — В М�?РОВЫХ координатах
        targetPositionOnFerry = Point(
            sprite->Left + 10, // + offsetX
            sprite->Top + offsetY
        );

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
            Point local(targetPositionOnFerry.X - sprite->Left,
                targetPositionOnFerry.Y - sprite->Top);
            currentMovingCar->Sprite->Parent = sprite;
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
    CarsParom(PictureBox^ spriteParom, int capacityInSlots)
        : Parom(spriteParom, capacityInSlots) {
    }
};