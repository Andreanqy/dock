#pragma once

using namespace System;
using namespace System::Drawing;
using namespace System::Windows::Forms;

public ref class Car
{
protected:
    PictureBox^ sprite;   // визуальный спрайт
    int slots;            // "вес" машины во вместимости парома (легк.=1, груз.=2)

public:
    int speed;            // пикселей за тик (всегда >0)
    Car(int speedPxPerTick, int capacitySlots) : speed(speedPxPerTick), slots(capacitySlots)
    {
        if (speed <= 0) speed = 1;  // защита от нулевой скорости
        sprite = gcnew PictureBox();
        sprite->BackColor = Color::Transparent;
        sprite->SizeMode = PictureBoxSizeMode::StretchImage;
        sprite->Width = 60;
        sprite->Height = 30;
    }

    property PictureBox^ Sprite { PictureBox^ get() { return sprite; } }
    property int Speed { int get() { return speed; } void set(int v) { speed = (v > 0 ? v : 1); } }
    property int Slots { int get() { return slots; } }

    // Плавное движение по X к targetX
    virtual bool MoveToX(int targetX)
    {
        if (sprite == nullptr) return true;
        if (sprite->Left == targetX) return true;

        // направление шага: вправо (+speed) или влево (-speed)
        int step = (sprite->Left < targetX) ? speed : -speed;
        int next = sprite->Left + step;

        // корректная проверка "перелёта": сравниваем по знаку шага
        if ((step > 0 && next > targetX) || (step < 0 && next < targetX))
            next = targetX;

        sprite->Left = next;
        return sprite->Left == targetX;
    }

    // Плавное движение по Y к targetY
    virtual bool MoveToY(int targetY)
    {
        if (sprite == nullptr) return true;
        if (sprite->Top == targetY) return true;

        int step = (sprite->Top < targetY) ? speed : -speed;
        int next = sprite->Top + step;

        if ((step > 0 && next > targetY) || (step < 0 && next < targetY))
            next = targetY;

        sprite->Top = next;
        return sprite->Top == targetY;
    }
};

public ref class Passcar : public Car
{
public:
    Passcar() : Car(15, 1)   // скорость легковой по умолчанию
    {
        // размеры/картинка задаём в MyForm через SetSpriteImage(...)
        sprite->Width = 80;
        sprite->Height = 40;
    }
};

public ref class Truck : public Car
{
public:
    Truck() : Car(10, 2)     // было 4 → ехал медленнее; теперь быстрее
    {
        sprite->Width = 200;
        sprite->Height = 40;
    }

    bool MoveToX(int targetX) override {
        int originalSpeed = speed;
        // speed = 3; // грузовики медленнее
        bool result = Car::MoveToX(targetX);
        // speed = originalSpeed;
        return result;
    }
};
