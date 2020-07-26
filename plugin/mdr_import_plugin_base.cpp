/*
Copyright (c) 2019-2020 Péter Magyar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "mdr_import_plugin_base.h"

#include "scene/resources/box_shape.h"
#include "scene/resources/capsule_shape.h"
#include "scene/resources/concave_polygon_shape.h"
#include "scene/resources/convex_polygon_shape.h"
#include "scene/resources/cylinder_shape.h"
#include "scene/resources/shape.h"
#include "scene/resources/sphere_shape.h"

const String MDRImportPluginBase::BINDING_MDR_IMPORT_TYPE = "Single,Single Merged,Multiple";

void MDRImportPluginBase::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "import_type", PROPERTY_HINT_ENUM, BINDING_MDR_IMPORT_TYPE), MDRImportPluginBase::MDR_IMPORT_TIME_SINGLE));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "collider_type", PROPERTY_HINT_ENUM, MeshDataResource::BINDING_STRING_COLLIDER_TYPE), MeshDataResource::COLLIDER_TYPE_NONE));

	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR3, "offset"), Vector3(0, 0, 0)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR3, "rotation"), Vector3(0, 0, 0)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR3, "scale"), Vector3(1, 1, 1)));
}

bool MDRImportPluginBase::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

Error MDRImportPluginBase::process_node(Node *n, const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	MDRImportPluginBase::MDRImportType type = static_cast<MDRImportPluginBase::MDRImportType>(static_cast<int>(p_options["import_type"]));

	switch (type) {
		case MDR_IMPORT_TIME_SINGLE: {
			return process_node_single(n, p_source_file, p_save_path, p_options, r_platform_variants, r_gen_files, r_metadata);
		}
		case MDR_IMPORT_TIME_SINGLE_MERGED: {
			ERR_FAIL_V_MSG(Error::ERR_UNAVAILABLE, "import type Single Merged is not yet implemented! " + p_source_file);
		}
		case MDR_IMPORT_TIME_MULTIPLE: {
			Ref<MeshDataResourceCollection> coll;
			coll.instance();

			process_node_multi(n, p_source_file, p_save_path, p_options, r_platform_variants, r_gen_files, r_metadata, coll);

			return ResourceSaver::save(p_save_path + "." + get_save_extension(), coll);
		}
	}

	return Error::ERR_PARSE_ERROR;
}

int MDRImportPluginBase::get_mesh_count(Node *n) {
	int count = 0;

	for (int i = 0; i < n->get_child_count(); ++i) {
		Node *c = n->get_child(i);

		if (Object::cast_to<MeshInstance>(c)) {
			++count;
		}

		count += get_mesh_count(c);
	}

	return count;
}

Error MDRImportPluginBase::process_node_single(Node *n, const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	MeshDataResource::ColliderType collider_type = static_cast<MeshDataResource::ColliderType>(static_cast<int>(p_options["collider_type"]));

	Vector3 scale = p_options["scale"];

	ERR_FAIL_COND_V(n == NULL, Error::ERR_PARSE_ERROR);

	for (int i = 0; i < n->get_child_count(); ++i) {
		Node *c = n->get_child(i);

		if (Object::cast_to<MeshInstance>(c)) {
			MeshInstance *mi = Object::cast_to<MeshInstance>(c);

			Ref<MeshDataResource> mdr = get_mesh(mi, p_options, collider_type, scale);

			ERR_FAIL_COND_V(!mdr.is_valid(), Error::ERR_PARSE_ERROR);

			return ResourceSaver::save(p_save_path + "." + get_save_extension(), mdr);
		}

		if (process_node_single(c, p_source_file, p_save_path, p_options, r_platform_variants, r_gen_files, r_metadata) == Error::OK) {
			return Error::OK;
		}
	}

	return Error::ERR_PARSE_ERROR;
}

Error MDRImportPluginBase::process_node_multi(Node *n, const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata, Ref<MeshDataResourceCollection> coll) {
	MeshDataResource::ColliderType collider_type = static_cast<MeshDataResource::ColliderType>(static_cast<int>(p_options["collider_type"]));

	Vector3 scale = p_options["scale"];

	ERR_FAIL_COND_V(n == NULL, Error::ERR_PARSE_ERROR);

	for (int i = 0; i < n->get_child_count(); ++i) {
		Node *c = n->get_child(i);

		if (Object::cast_to<MeshInstance>(c)) {
			MeshInstance *mi = Object::cast_to<MeshInstance>(c);

			Ref<MeshDataResource> mdr = get_mesh(mi, p_options, collider_type, scale);

			String node_name = n->get_name();

			node_name = node_name.to_lower();

			String filename = p_source_file.get_basename() + "_" + node_name + "." + get_save_extension();

			Error err = ResourceSaver::save(filename, mdr);

			Ref<MeshDataResource> mdrl = ResourceLoader::load(filename);

			coll->add_mdr(mdrl);

			if (err != Error::OK) {
				return err;
			}
		}

		process_node_multi(c, p_source_file, p_save_path, p_options, r_platform_variants, r_gen_files, r_metadata, coll);
	}

	return Error::OK;
}

Ref<MeshDataResource> MDRImportPluginBase::get_mesh(MeshInstance *mi, const Map<StringName, Variant> &p_options, MeshDataResource::ColliderType collider_type, Vector3 scale) {
	Ref<ArrayMesh> mesh = mi->get_mesh();

	if (mesh.is_valid()) {
		Ref<MeshDataResource> mdr;
		mdr.instance();

		Array arrays = mesh->surface_get_arrays(0);

		mdr->set_array(apply_transforms(arrays, p_options));

		if (collider_type == MeshDataResource::COLLIDER_TYPE_TRIMESH_COLLISION_SHAPE) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<Shape> shape = m->create_trimesh_shape();

			if (!shape.is_null()) {
				mdr->add_collision_shape(Transform(), scale_shape(shape, scale));
			}
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_SINGLE_CONVEX_COLLISION_SHAPE) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<Shape> shape = mesh->create_convex_shape();

			if (!shape.is_null()) {
				mdr->add_collision_shape(Transform(), scale_shape(shape, scale));
			}
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_MULTIPLE_CONVEX_COLLISION_SHAPES) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Vector<Ref<Shape> > shapes = mesh->convex_decompose();

			for (int j = 0; j < shapes.size(); ++j) {
				scale_shape(shapes[j], scale);
			}

			for (int j = 0; j < shapes.size(); ++j) {
				mdr->add_collision_shape(Transform(), shapes[j]);
			}
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_APPROXIMATED_BOX) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<BoxShape> shape;
			shape.instance();

			AABB aabb = m->get_aabb();
			Vector3 size = aabb.get_size();

			shape->set_extents(size * 0.5);

			Vector3 pos = aabb.position;
			pos += size / 2.0;

			Transform t;
			t.origin = pos;

			mdr->add_collision_shape(t, shape);
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_APPROXIMATED_CAPSULE) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<CapsuleShape> shape;
			shape.instance();

			AABB aabb = m->get_aabb();
			Vector3 size = aabb.get_size();

			shape->set_height(size.y * 0.5);
			shape->set_radius(MIN(size.x, size.z) * 0.5);

			Vector3 pos = aabb.position;
			pos += size / 2.0;

			Transform t = Transform(Basis().rotated(Vector3(1, 0, 0), M_PI_2));
			t.origin = pos;

			mdr->add_collision_shape(t, shape);
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_APPROXIMATED_CYLINDER) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<CylinderShape> shape;
			shape.instance();

			AABB aabb = m->get_aabb();
			Vector3 size = aabb.get_size();

			shape->set_height(size.y * 0.5);
			shape->set_radius(MIN(size.x, size.z) * 0.5);

			Vector3 pos = aabb.position;
			pos += size / 2.0;

			Transform t;
			t.origin = pos;

			mdr->add_collision_shape(t, shape);
		} else if (collider_type == MeshDataResource::COLLIDER_TYPE_APPROXIMATED_SPHERE) {
			Ref<ArrayMesh> m;
			m.instance();
			m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, mdr->get_array());

			Ref<SphereShape> shape;
			shape.instance();

			AABB aabb = m->get_aabb();
			Vector3 size = aabb.get_size();

			shape->set_radius(MIN(size.x, MIN(size.y, size.z)) * 0.5);

			Vector3 mid = aabb.get_size() / 2.0;

			Transform t;
			t.origin = aabb.position + mid;

			mdr->add_collision_shape(t, shape);
		}

		return mdr;
	}

	return Ref<MeshDataResource>();
}

Array MDRImportPluginBase::apply_transforms(Array &array, const Map<StringName, Variant> &p_options) {
	Vector3 offset = p_options["offset"];
	Vector3 rotation = p_options["rotation"];
	Vector3 scale = p_options["scale"];

	Transform transform = Transform(Basis(rotation).scaled(scale), offset);

	PoolVector3Array verts = array.get(Mesh::ARRAY_VERTEX);

	for (int i = 0; i < verts.size(); ++i) {
		Vector3 vert = verts[i];

		vert = transform.xform(vert);

		verts.set(i, (vert));
	}

	PoolVector3Array normals = array.get(Mesh::ARRAY_NORMAL);

	for (int i = 0; i < normals.size(); ++i) {
		Vector3 normal = normals[i];

		normal = transform.basis.xform(normal);

		normals.set(i, normal);
	}
	/*
	Array tangents = array.get(Mesh::ARRAY_TANGENT);

	if (tangents.size() == verts.size() * 4) {

		for (int i = 0; i < verts.size(); ++i) {

			Plane p(tangents[i * 4 + 0], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]);

			Vector3 tangent = p.normal;

			tangent = transform.basis.xform(tangent);

			tangents.set(i, tangent);
		}
	}
*/
	array.set(Mesh::ARRAY_VERTEX, verts);
	array.set(Mesh::ARRAY_NORMAL, normals);
	//array.set(Mesh::ARRAY_TANGENT, tangents);

	return array;
}

Ref<Shape> MDRImportPluginBase::scale_shape(Ref<Shape> shape, const Vector3 &scale) {

	if (shape.is_null())
		return shape;

	if (Object::cast_to<SphereShape>(*shape)) {

		Ref<SphereShape> ss = shape;

		ss->set_radius(ss->get_radius() * MAX(scale.x, MAX(scale.y, scale.z)));
	}

	if (Object::cast_to<BoxShape>(*shape)) {

		Ref<BoxShape> bs = shape;

		bs->set_extents(bs->get_extents() * scale);
	}

	if (Object::cast_to<CapsuleShape>(*shape)) {

		Ref<CapsuleShape> cs = shape;

		float sc = MAX(scale.x, MAX(scale.y, scale.z));

		cs->set_radius(cs->get_radius() * sc);
		cs->set_height(cs->get_height() * sc);
	}

	if (Object::cast_to<CylinderShape>(*shape)) {

		Ref<CylinderShape> cs = shape;

		float sc = MAX(scale.x, MAX(scale.y, scale.z));

		cs->set_radius(cs->get_radius() * sc);
		cs->set_height(cs->get_height() * sc);
	}

	if (Object::cast_to<ConcavePolygonShape>(*shape)) {

		Ref<ConcavePolygonShape> cps = shape;

		PoolVector3Array arr = cps->get_faces();

		Basis b = Basis().scaled(scale);

		for (int i = 0; i < arr.size(); ++i) {
			arr.set(i, b.xform(arr[i]));
		}

		cps->set_faces(arr);
	}

	if (Object::cast_to<ConvexPolygonShape>(*shape)) {

		Ref<ConvexPolygonShape> cps = shape;

		PoolVector3Array arr = cps->get_points();

		Basis b = Basis().scaled(scale);

		for (int i = 0; i < arr.size(); ++i) {
			arr.set(i, b.xform(arr[i]));
		}

		cps->set_points(arr);
	}

	return shape;
}

MDRImportPluginBase::MDRImportPluginBase() {
}

MDRImportPluginBase::~MDRImportPluginBase() {
}