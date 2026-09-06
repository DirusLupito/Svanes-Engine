#pragma once

namespace svanes {

/**
 * Represents a vector in 2D space.
 *
 * FIELDS:
 * - x: The x-component of the vector.
 * - y: The y-component of the vector.
 */
struct Vector2D {
    float x = 0.0F;
    float y = 0.0F;

    /**
     * Adds two Vector2D instances together and returns the result.
     * 
     * @param other The other Vector2D to add.
     * @return Vector2D The result of the addition.
     */
    Vector2D operator+(Vector2D other) const
    {
        return {x + other.x, y + other.y};
    }

    /**
     * Subtracts one Vector2D from another and returns the result.
     * 
     * @param other The other Vector2D to subtract.
     * @return Vector2D The result of the subtraction.
     */
    Vector2D operator-(Vector2D other) const
    {
        return {x - other.x, y - other.y};
    }

    /**
     * Multiplies a Vector2D by a scalar and returns the result.
     * 
     * @param scalar The scalar to multiply by.
     * @return Vector2D The result of the multiplication.
     */
    Vector2D operator*(float scalar) const
    {
        return {x * scalar, y * scalar};
    }

    /**
     * Divides a Vector2D by a scalar and returns the result.
     * 
     * @param scalar The scalar to divide by.
     * @return Vector2D The result of the division.
     */
    Vector2D operator/(float scalar) const
    {
        return {x / scalar, y / scalar};
    }

    /**
     * Adds another Vector2D to this instance and returns the result.
     * 
     * @param other The other Vector2D to add.
     * @return Vector2D& A reference to this instance after addition.
     */
    Vector2D& operator+=(Vector2D other)
    {
        *this = *this + other;
        return *this;
    }
};

}
