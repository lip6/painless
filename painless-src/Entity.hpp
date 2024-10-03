#pragma once

struct Entity {
    int id;

    Entity(int _id) : id(_id)
    {
    }

    virtual ~Entity()
    {
    }

    /* Getter just in case */
    int getId() { return this->id; }

    /* Setter just in case*/
    void setId(int _id) { this->id = _id;}
};