#pragma once

#include "Parom.h"
#include "human.h"
#include "Car.h"
#include "Delegates.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Windows::Forms;
using namespace System::Drawing;

namespace Project13 {

    public ref class MyForm : public System::Windows::Forms::Form
    {
    private:
        // ===================== НАСТРОЙКИ СЦЕНЫ =====================
        // Стартовые точки спавна (из "углов" дорог)
        literal int LEFT_SPAWN_X_START = 0;
        literal int LEFT_SPAWN_Y = 470;
        literal int RIGHT_SPAWN_X_START = 1300;
        literal int RIGHT_SPAWN_Y = 150;

        // Размеры машин
        literal int PASS_W = 80;
        literal int PASS_H = 40;
        literal int TRUCK_W = 200;
        literal int TRUCK_H = 40;

        // Паром (старт у левого берега)
        literal int FERRY_W = 225;
        literal int FERRY_H = 80;
        literal int FERRY_LEFT_X = 460;
        literal int FERRY_LEFT_Y = 500 - FERRY_H / 2;

        literal int FERRY_RIGHT_X = 580;  // Примерная координата, подбери под свой фон (обозначает координату X парома для правого берега)
        literal int FERRY_RIGHT_Y = 140;  // Примерная координата, подбери под свой фон (обозначает координату Y парома для правого берега)

        // ===================== НАСТРОЙКИ ОЧЕРЕДЕЙ =====================
        // Точки очередей у шлагбаумов (куда машины выстраиваются)
        literal int LEFT_QUEUE_X = FERRY_LEFT_X - 120;        // точка у левого трапа
        literal int LEFT_QUEUE_Y = LEFT_SPAWN_Y;             // по Y остаёмся на дороге

        // Правый берег
        literal int RIGHT_QUEUE_X = FERRY_LEFT_X + 470; // временно "примерно справа"
        literal int RIGHT_QUEUE_Y = RIGHT_SPAWN_Y;

        // Дистанция между машинами в очереди (в пикселях)
        literal int QUEUE_GAP = 14;

        // ========================================================================

        // Поля сцены
        List<Car^>^ leftCars;
        List<Car^>^ rightCars;

        SecurityGuard^ guardLeft;
        SecurityGuard^ guardRight;

        PictureBox^ paromSprite;

        Timer^ checkParomTimer; // тиканьё «мира»

        // Добавляем поля для движения парома
        Timer^ paromMovementTimer;
        int paromTargetX;
        int paromTargetY;
        int paromStep;
    public: static CarsParom^ parom;
    private: System::Windows::Forms::Label^ CounterLabel;
    private: System::Windows::Forms::TrackBar^ trackBar1;
    private: System::Windows::Forms::Button^ button1;

        System::Random^ rng;

        // Утилита поиска спрайта в .\Sprites / ..\Sprites / ..\..\Sprites
        System::String^ SpritePath(System::String^ file) {
            using namespace System::IO;
            array<System::String^>^ roots = gcnew array<System::String^>{
                Application::StartupPath,
                    Path::Combine(Application::StartupPath, ".."),
                    Path::Combine(Application::StartupPath, "..", "..")
            };
            for each (System::String ^ r in roots) {
                System::String^ p = Path::Combine(r, "Sprites", file);
                if (File::Exists(p)) return p;
            }
            return nullptr;
        }

        // Устанавливаем изображение
        void SetSpriteImage(PictureBox^ pb, System::String^ file, bool faceRight, int w, int h, Color fallbackColor)
        {
            auto p = SpritePath(file);
            if (p != nullptr)
            {
                auto img = Image::FromFile(p);
                if (faceRight) img->RotateFlip(RotateFlipType::RotateNoneFlipX);
                pb->Image = img;
                pb->BackColor = Color::Transparent;
                pb->SizeMode = PictureBoxSizeMode::StretchImage;
            }
            else throw String::Format("Файл {file} не найден!");
            pb->Width = w; pb->Height = h;
        }

        // ------------------ Добавление транспорта ------------------
        void AddPassLeft(Object^, EventArgs^) {
            auto car = gcnew Passcar();
            System::String^ file = (rng->Next(2) == 0) ? "GreenCar.png" : "RedCar.png";
            SetSpriteImage(car->Sprite, file, true, PASS_W, PASS_H, Color::Yellow);

            // спавним из "левого угла"
            car->Sprite->Left = LEFT_SPAWN_X_START - car->Sprite->Width;
            car->Sprite->Top = LEFT_SPAWN_Y - car->Sprite->Height / 2;

            leftCars->Add(car);
            this->groupBox1->Controls->Add(car->Sprite);
            car->Sprite->BringToFront();
        }

        void AddTruckLeft(Object^, EventArgs^) {
            auto car = gcnew Truck();
            SetSpriteImage(car->Sprite, "Truck.png", true, TRUCK_W, TRUCK_H, Color::Orange);

            car->Sprite->Left = LEFT_SPAWN_X_START - car->Sprite->Width;
            car->Sprite->Top = LEFT_SPAWN_Y - car->Sprite->Height / 2;

            leftCars->Add(car);
            this->groupBox1->Controls->Add(car->Sprite);
            car->Sprite->BringToFront();
        }

        void AddPassRight(Object^, EventArgs^) {
            auto car = gcnew Passcar();
            System::String^ file = (rng->Next(2) == 0) ? "GreenCar.png" : "RedCar.png";
            SetSpriteImage(car->Sprite, file, false, PASS_W, PASS_H, Color::Yellow);

            // спавним из "правого угла"
            car->Sprite->Left = RIGHT_SPAWN_X_START;
            car->Sprite->Top = RIGHT_SPAWN_Y - car->Sprite->Height / 2;

            rightCars->Add(car);
            this->groupBox1->Controls->Add(car->Sprite);
            car->Sprite->BringToFront();
        }

        void AddTruckRight(Object^, EventArgs^) {
            auto car = gcnew Truck();
            SetSpriteImage(car->Sprite, "Truck.png", /*faceRight=*/false, TRUCK_W, TRUCK_H, Color::Orange);

            car->Sprite->Left = RIGHT_SPAWN_X_START;
            car->Sprite->Top = RIGHT_SPAWN_Y - car->Sprite->Height / 2;

            rightCars->Add(car);
            this->groupBox1->Controls->Add(car->Sprite);
            car->Sprite->BringToFront();
        }

        // ------------------ Тик мира: движение колонн к шлагбаумам ------------------

        void OnCheckParom(System::Object^, System::EventArgs^)
        {
            // Левый берег: головная встаёт у трапа, остальные — с зазором за ней
            if (leftCars->Count > 0)
            {
                // головная
                int headTargetX = LEFT_QUEUE_X - leftCars[0]->Sprite->Width;
                leftCars[0]->MoveToY(LEFT_QUEUE_Y - leftCars[0]->Sprite->Height / 2);
                leftCars[0]->MoveToX(headTargetX);

                // хвост колонны
                for (int i = 1; i < leftCars->Count; ++i)
                {
                    auto prev = leftCars[i - 1]->Sprite;
                    int tx = prev->Left - leftCars[i]->Sprite->Width - QUEUE_GAP;
                    leftCars[i]->MoveToY(LEFT_QUEUE_Y - leftCars[i]->Sprite->Height / 2);
                    leftCars[i]->MoveToX(tx);
                }
            }

            // Правый берег: зеркальная логика
            if (rightCars->Count > 0) {
                // головная
                int headTargetX = RIGHT_QUEUE_X;
                rightCars[0]->MoveToY(RIGHT_QUEUE_Y - rightCars[0]->Sprite->Height / 2);
                rightCars[0]->MoveToX(headTargetX);

                // хвост
                for (int i = 1; i < rightCars->Count; ++i) {
                    auto prev = rightCars[i - 1]->Sprite;
                    int tx = prev->Left + prev->Width + QUEUE_GAP;
                    rightCars[i]->MoveToY(RIGHT_QUEUE_Y - rightCars[i]->Sprite->Height / 2);
                    rightCars[i]->MoveToX(tx);
                }
            }
            CheckQueues();
        }

        void SetupEventHandlers()
        {
            // Подписка на события охранников
            guardLeft->OnQueueReady += gcnew EventHandler(this, &MyForm::OnLeftQueueReady);
            guardRight->OnQueueReady += gcnew EventHandler(this, &MyForm::OnRightQueueReady);
            guardLeft->OnQueueEmpty += gcnew EventHandler(this, &MyForm::OnLeftQueueEmpty);
            guardRight->OnQueueEmpty += gcnew EventHandler(this, &MyForm::OnRightQueueEmpty);

            // Подписка на события парома (ТОЛЬКО основные)
            parom->OnParomStateChanged += gcnew ParomStateEventHandler(this, &MyForm::OnParomStateChanged);
            parom->OnUnloadingFinished += gcnew SimpleEventHandler(this, &MyForm::OnUnloadingFinishedHandler);
            parom->OnCarLoadingProgress += gcnew System::EventHandler(this, &MyForm::OnCarLoadingProgress);
        }

        void CheckQueues()
        {
            // Передаём координаты очередей
            guardLeft->CheckQueue(leftCars, LEFT_QUEUE_X, LEFT_QUEUE_Y - PASS_H / 2, parom->isLeftSide);
            guardRight->CheckQueue(rightCars, RIGHT_QUEUE_X, RIGHT_QUEUE_Y - PASS_H / 2, parom->isLeftSide);
        }

        void OnLeftQueueReady(Object^ sender, EventArgs^ e)
        {
            if (parom->CanLoadFromShore(true)) parom->StartAsyncLoading(leftCars);
        }

        void OnRightQueueReady(Object^ sender, EventArgs^ e)
        {
            if (parom->CanLoadFromShore(false) && !parom->isLeftSide) parom->StartAsyncLoading(rightCars);
        }

        void OnLeftQueueEmpty(Object^ sender, EventArgs^ e)
        {
            // Если загрузка идёт и очередь закончилась - можно завершать загрузку
            if (parom->State == Parom::ParomState::Loading && parom->isLeftSide) {
                // Логика завершения загрузки при пустой очереди
            }
        }

        void OnRightQueueEmpty(Object^ sender, EventArgs^ e)
        {
            // Аналогично для правого берега
            if (parom->State == Parom::ParomState::Loading && !parom->isLeftSide) {
                // Логика завершения загрузки при пустой очереди
            }
        }

        void OnParomStateChanged(Parom^ parom)
        {
            // Обновляем CounterLabel
            CounterLabel->Text = String::Format("{0}/{1}", parom->FilledSlots, parom->CapacitySlots);

            // Дополнительная логика при смене состояний
            switch (parom->State) {
            case Parom::ParomState::MovingToDest:
                // Запускаем анимацию движения парома
                StartParomMovement();
                break;
            case Parom::ParomState::Unloading:
                // Запускаем анимацию разгрузки парома
                // StartParomUnloading();
                break;
            case Parom::ParomState::Loading:
                break;
            case Parom::ParomState::Waiting:;
                break;
            }
        }

        void OnUnloadingFinishedHandler()
        {
            // ВЫГРУЖАЕМ МАШИНЫ
            UnloadCars();

            // Выгрузка завершена - паром пустой и в состоянии Waiting
            CounterLabel->Text = String::Format("{0}/{1}", 0, parom->CapacitySlots);

            // Может начать новую загрузку если есть очередь
            CheckQueues();

            Console::WriteLine("Выгрузка завершена. Паром готов к новой загрузке.");
        }

        void StartParomMovement()
        {
            if (paromMovementTimer != nullptr)
            {
                paromMovementTimer->Stop();
                paromMovementTimer = nullptr;
            }

            paromMovementTimer = gcnew Timer();
            paromMovementTimer->Interval = 30; // Быстрее для тестирования
            paromStep = 15;
            paromTargetX = parom->isLeftSide ? FERRY_RIGHT_X : FERRY_LEFT_X;
            paromTargetY = parom->isLeftSide ? FERRY_RIGHT_Y : FERRY_LEFT_Y;

            Console::WriteLine("Начинаем движение парома от {0} к {1}",
                parom->Sprite->Left, paromTargetX);

            paromMovementTimer->Tick += gcnew EventHandler(this, &MyForm::OnParomMovementTick);
            paromMovementTimer->Start();
        }

        void OnParomMovementTick(Object^ sender, EventArgs^ e)
        {
            int currentX = parom->Sprite->Left;
            int currentY = parom->Sprite->Top;

            int dirX = (paromTargetX > currentX) ? 1 : -1;
            int dirY = (paromTargetY > currentY) ? 1 : -1;

            int newX = currentX + paromStep * dirX;
            int newY = currentY + paromStep * dirY;

            // Проверяем, не перескочили ли цель
            if ((dirX > 0 && newX >= paromTargetX) || (dirX < 0 && newX <= paromTargetX)) newX = paromTargetX;
            if ((dirY > 0 && newY >= paromTargetY) || (dirY < 0 && newY <= paromTargetY)) newY = paromTargetY;

            double dx = paromTargetX - currentX;
            double dy = paromTargetY - currentY;
            double dist = Math::Sqrt(dx * dx + dy * dy);

            if (dist < paromStep) {
                parom->Sprite->Left = paromTargetX;
                parom->Sprite->Top = paromTargetY;
            }
            else {
                parom->Sprite->Left = currentX + (int)(paromStep * dx / dist);
                parom->Sprite->Top = currentY + (int)(paromStep * dy / dist);
            }

            // Отладочный вывод
            Console::WriteLine("Паром движется: {0} -> {1} (цель: {2})",
                currentX, newX, paromTargetX);

            if (newX == paromTargetX && newY == paromTargetY) {
                paromMovementTimer->Stop();

                // НАЧИНАЕМ ВЫГРУЗКУ
                parom->StartAsyncUnloading();
                //UnloadCars();          // выгружаем машины на текущий берег
                //parom->FinishUnloading();

                //parom->isLeftSide = !parom->isLeftSide; // инверсия стороны после выгрузки
            }

        }

        void UnloadCars()
        {
            // Получаем список машин на пароме
            List<Control^>^ onboardCars = parom->Onboard();

            // Вспомогательный список для новых машин в очереди
            List<Car^>^ targetList = parom->isLeftSide ? rightCars : leftCars;
            int queueY = parom->isLeftSide ? RIGHT_QUEUE_Y : LEFT_QUEUE_Y;
            int startX = parom->isLeftSide ? FERRY_LEFT_X + FERRY_W + 50 : FERRY_RIGHT_X - 50;

            for each (Control ^ carSprite in onboardCars)
            {
                // Перемещаем спрайт на форму
                carSprite->Parent = this->groupBox1;

                // Позиционируем спрайт на берегу
                carSprite->Top = queueY - carSprite->Height / 2;

                if (parom->isLeftSide)
                    carSprite->Left = startX;   // левый берег — машины поехали вправо
                else
                    carSprite->Left = startX;   // правый берег — машины поехали влево

                // Находим объект Car, которому принадлежит этот спрайт
                Car^ foundCar = nullptr;
                for each (Car ^ c in leftCars)
                {
                    if (c->Sprite == carSprite) { foundCar = c; leftCars->Remove(c); break; }
                }
                if (!foundCar)
                {
                    for each (Car ^ c in rightCars)
                    {
                        if (c->Sprite == carSprite) { foundCar = c; rightCars->Remove(c); break; }
                    }
                }

                // Если нашли объект Car, добавляем его в очередь нового берега
                if (foundCar)
                    targetList->Add(foundCar);
            }

            // Очищаем паром
            onboardCars->Clear();
        }


        Void OnCarLoadingProgress(System::Object^ sender, System::EventArgs^ e)
        {
            // обновляем счётчик ровно в момент заезда
            CounterLabel->Text = System::String::Format("{0}/6", parom->UsedSlots);
        }

        // (пока пусто — в следующих шагах)
        void OnLeftLoadAllowed() {}
        void OnRightLoadAllowed() {}
        void OnUnloadStart() {}

        void OnSpeedChanged(Object^ sender, EventArgs^ e) {
            UpdateSpeed(trackBar1->Value);
        }

        void UpdateSpeed(int speedLevel) {
            // Преобразуем уровень в множитель скорости
            double speedMultiplier;

            switch (speedLevel) {
            case 1:
                speedMultiplier = 0.5;
                break;
            case 2:
                speedMultiplier = 1.0;
                break;
            case 3:
                speedMultiplier = 1.5;
                break;
            default:
                speedMultiplier = 1.0;
                break;
            }

            // Меняем только интервал таймера, НЕ трогаем CounterLabel
            checkParomTimer->Interval = (int)(30 / speedMultiplier);

            // CounterLabel остаётся неизменным - он только для счётчика парома
        }

    public:
        MyForm(void)
        {
            InitializeComponent();

            rng = gcnew System::Random();
            leftCars = gcnew List<Car^>();
            rightCars = gcnew List<Car^>();

            // Паром
            paromSprite = gcnew PictureBox();
            paromSprite->SizeMode = PictureBoxSizeMode::StretchImage;
            paromSprite->Width = FERRY_W; paromSprite->Height = FERRY_H;
            paromSprite->Left = FERRY_LEFT_X;
            paromSprite->Top = FERRY_LEFT_Y;

            auto pf = SpritePath("ParomForCars.png");
            if (pf != nullptr) paromSprite->ImageLocation = pf;
            else { paromSprite->BackColor = Color::SaddleBrown; paromSprite->BorderStyle = BorderStyle::FixedSingle; }

            this->groupBox1->Controls->Add(paromSprite);
            parom = gcnew CarsParom(paromSprite, 6);

            // Охранники (координаты/размер — подберите сами)
            guardLeft = gcnew SecurityGuard(SpritePath("guard.png"));
            guardRight = gcnew SecurityGuard(SpritePath("guard.png"));

            // Пример: ваши текущие координаты
            guardLeft->Sprite->Left = 365;   guardLeft->Sprite->Top = 551;
            guardRight->Sprite->Left = 884;  guardRight->Sprite->Top = 118;

            guardLeft->Sprite->SizeMode = PictureBoxSizeMode::StretchImage;
            guardRight->Sprite->SizeMode = PictureBoxSizeMode::StretchImage;
            guardLeft->Sprite->Width = 16;   guardLeft->Sprite->Height = 16;   // ПРАВЬТЕ
            guardRight->Sprite->Width = 16;  guardRight->Sprite->Height = 16;  // ПРАВЬТЕ

            this->groupBox1->Controls->Add(guardLeft->Sprite);
            this->groupBox1->Controls->Add(guardRight->Sprite);

            SetupEventHandlers();

            guardLeft->OnLoadAllowed += gcnew GuardEventHandler(this, &MyForm::OnLeftLoadAllowed);
            guardRight->OnLoadAllowed += gcnew GuardEventHandler(this, &MyForm::OnRightLoadAllowed);
            parom->OnUnloadStart += gcnew SimpleEventHandler(this, &MyForm::OnUnloadStart);

            // Таймер «мира» — для плавного движения лучше 16..33 мс
            checkParomTimer = gcnew Timer();
            checkParomTimer->Interval = 30;
            checkParomTimer->Tick += gcnew EventHandler(this, &MyForm::OnCheckParom);
            checkParomTimer->Start();

            // ========== НАСТРОЙКА TRACKBAR ==========

            trackBar1->Minimum = 1;
            trackBar1->Maximum = 3;    // 3 уровня скорости
            trackBar1->Value = 2;      // 1x (нормальная скорость)
            trackBar1->SmallChange = 1;
            trackBar1->LargeChange = 1;
            trackBar1->TickStyle = TickStyle::BottomRight;
            trackBar1->TickFrequency = 1;

            trackBar1->ValueChanged += gcnew EventHandler(this, &MyForm::OnSpeedChanged);
            UpdateSpeed(trackBar1->Value);
        }

    protected:
        ~MyForm()
        {
            if (components) delete components;
        }

    private: System::Windows::Forms::GroupBox^ groupBox1;
    private: System::Windows::Forms::Button^ ButtonAddTR;
    private: System::Windows::Forms::Button^ ButtonAddTL;
    private: System::Windows::Forms::Button^ ButtonAddCR;
    private: System::Windows::Forms::Button^ ButtonAddCL;
    private: System::ComponentModel::IContainer^ components;

#pragma region Windows Form Designer generated code
           void InitializeComponent(void)
           {
               System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(MyForm::typeid));
               this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
               this->CounterLabel = (gcnew System::Windows::Forms::Label());
               this->trackBar1 = (gcnew System::Windows::Forms::TrackBar());
               this->ButtonAddTR = (gcnew System::Windows::Forms::Button());
               this->ButtonAddTL = (gcnew System::Windows::Forms::Button());
               this->ButtonAddCR = (gcnew System::Windows::Forms::Button());
               this->ButtonAddCL = (gcnew System::Windows::Forms::Button());
               this->button1 = (gcnew System::Windows::Forms::Button());
               this->groupBox1->SuspendLayout();
               (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->trackBar1))->BeginInit();
               this->SuspendLayout();
               // 
               // groupBox1
               // 
               this->groupBox1->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom)
                   | System::Windows::Forms::AnchorStyles::Left)
                   | System::Windows::Forms::AnchorStyles::Right));
               this->groupBox1->BackgroundImage = (cli::safe_cast<System::Drawing::Image^>(resources->GetObject(L"groupBox1.BackgroundImage")));
               this->groupBox1->BackgroundImageLayout = System::Windows::Forms::ImageLayout::Stretch;
               this->groupBox1->Controls->Add(this->button1);
               this->groupBox1->Controls->Add(this->CounterLabel);
               this->groupBox1->Controls->Add(this->trackBar1);
               this->groupBox1->Controls->Add(this->ButtonAddTR);
               this->groupBox1->Controls->Add(this->ButtonAddTL);
               this->groupBox1->Controls->Add(this->ButtonAddCR);
               this->groupBox1->Controls->Add(this->ButtonAddCL);
               this->groupBox1->Location = System::Drawing::Point(0, 0);
               this->groupBox1->Name = L"groupBox1";
               this->groupBox1->Size = System::Drawing::Size(1264, 683);
               this->groupBox1->TabIndex = 3;
               this->groupBox1->TabStop = false;
               // 
               // CounterLabel
               // 
               this->CounterLabel->Anchor = System::Windows::Forms::AnchorStyles::Top;
               this->CounterLabel->AutoSize = true;
               this->CounterLabel->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 18, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point,
                   static_cast<System::Byte>(0)));
               this->CounterLabel->Location = System::Drawing::Point(609, 16);
               this->CounterLabel->Name = L"CounterLabel";
               this->CounterLabel->Size = System::Drawing::Size(46, 29);
               this->CounterLabel->TabIndex = 9;
               this->CounterLabel->Text = L"0/6";
               // 
               // trackBar1
               // 
               this->trackBar1->Anchor = System::Windows::Forms::AnchorStyles::Bottom;
               this->trackBar1->Location = System::Drawing::Point(486, 632);
               this->trackBar1->Name = L"trackBar1";
               this->trackBar1->Size = System::Drawing::Size(288, 45);
               this->trackBar1->TabIndex = 8;
               // 
               // ButtonAddTR
               // 
               this->ButtonAddTR->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
               this->ButtonAddTR->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 14.25F));
               this->ButtonAddTR->Location = System::Drawing::Point(1080, 441);
               this->ButtonAddTR->Name = L"ButtonAddTR";
               this->ButtonAddTR->Size = System::Drawing::Size(163, 73);
               this->ButtonAddTR->TabIndex = 6;
               this->ButtonAddTR->Text = L"Добавить\r\nгрузовик справа";
               this->ButtonAddTR->UseVisualStyleBackColor = true;
               this->ButtonAddTR->Click += gcnew System::EventHandler(this, &MyForm::ButtonAddTR_Click);
               // 
               // ButtonAddTL
               // 
               this->ButtonAddTL->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 14.25F));
               this->ButtonAddTL->Location = System::Drawing::Point(254, 205);
               this->ButtonAddTL->Name = L"ButtonAddTL";
               this->ButtonAddTL->Size = System::Drawing::Size(160, 76);
               this->ButtonAddTL->TabIndex = 5;
               this->ButtonAddTL->Text = L"Добавить\r\nгрузовик слева";
               this->ButtonAddTL->UseVisualStyleBackColor = true;
               this->ButtonAddTL->Click += gcnew System::EventHandler(this, &MyForm::ButtonAddTL_Click);
               // 
               // ButtonAddCR
               // 
               this->ButtonAddCR->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right));
               this->ButtonAddCR->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 14.25F));
               this->ButtonAddCR->Location = System::Drawing::Point(885, 441);
               this->ButtonAddCR->Name = L"ButtonAddCR";
               this->ButtonAddCR->Size = System::Drawing::Size(168, 73);
               this->ButtonAddCR->TabIndex = 2;
               this->ButtonAddCR->Text = L"Добавить\r\nмашину справа";
               this->ButtonAddCR->UseVisualStyleBackColor = true;
               this->ButtonAddCR->Click += gcnew System::EventHandler(this, &MyForm::ButtonAddCR_Click);
               // 
               // ButtonAddCL
               // 
               this->ButtonAddCL->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 14.25F));
               this->ButtonAddCL->Location = System::Drawing::Point(51, 205);
               this->ButtonAddCL->Name = L"ButtonAddCL";
               this->ButtonAddCL->Size = System::Drawing::Size(160, 76);
               this->ButtonAddCL->TabIndex = 1;
               this->ButtonAddCL->Text = L"Добавить\r\nмашину слева";
               this->ButtonAddCL->UseVisualStyleBackColor = true;
               this->ButtonAddCL->Click += gcnew System::EventHandler(this, &MyForm::ButtonAddCL_Click);
               // 
               // button1
               // 
               this->button1->Location = System::Drawing::Point(486, 21);
               this->button1->Name = L"button1";
               this->button1->Size = System::Drawing::Size(75, 23);
               this->button1->TabIndex = 10;
               this->button1->Text = L"check";
               this->button1->UseVisualStyleBackColor = true;
               this->button1->Click += gcnew System::EventHandler(this, &MyForm::button1_Click);
               // 
               // MyForm
               // 
               this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
               this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
               this->BackgroundImageLayout = System::Windows::Forms::ImageLayout::Stretch;
               this->ClientSize = System::Drawing::Size(1264, 681);
               this->Controls->Add(this->groupBox1);
               this->DoubleBuffered = true;
               this->Icon = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
               this->MaximizeBox = false;
               this->Name = L"MyForm";
               this->Text = L"Переправа через паром";
               this->Load += gcnew System::EventHandler(this, &MyForm::MyForm_Load);
               this->groupBox1->ResumeLayout(false);
               this->groupBox1->PerformLayout();
               (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->trackBar1))->EndInit();
               this->ResumeLayout(false);

           }
#pragma endregion

    private: System::Void btnExit_Click(System::Object^ sender, System::EventArgs^ e) {
        this->Close();
    }
    private: System::Void MyForm_Load(System::Object^ sender, System::EventArgs^ e) {}

    private: System::Void ButtonAddCL_Click(System::Object^ sender, System::EventArgs^ e) {
        AddPassLeft(sender, e);
    }
    private: System::Void ButtonAddCR_Click(System::Object^ sender, System::EventArgs^ e) {
        AddPassRight(sender, e);
    }
    private: System::Void ButtonAddTL_Click(System::Object^ sender, System::EventArgs^ e) {
        AddTruckLeft(sender, e);
    }
    private: System::Void ButtonAddTR_Click(System::Object^ sender, System::EventArgs^ e) {
        AddTruckRight(sender, e);
    }
    private: System::Void button1_Click(System::Object^ sender, System::EventArgs^ e) {
        auto check = parom;
        int a = 0;
    }
};
}