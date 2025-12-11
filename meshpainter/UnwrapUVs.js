import * as THREE from "three"

// Smart UV Unwrapping (Angle-based Chart Generation)
export default function UnwrapUVs(geometry) {
    try {
        console.log("Starting Smart UV Unwrap...");
        // 1. Ensure connectivity data exists (Indices)
        let isIndexed = !!geometry.index;

        const posAttr = geometry.attributes.position;
        const indexAttr = geometry.index;
        const vertexCount = posAttr.count;
        const triCount = isIndexed ? indexAttr.count / 3 : vertexCount / 3;

        // --- Helper: Get Vertex Data ---
        const v1 = new THREE.Vector3();
        const v2 = new THREE.Vector3();
        const v3 = new THREE.Vector3();

        const getTriIndices = (ti) => {
            if (isIndexed) {
                const arr = indexAttr.array;
                return [arr[ti * 3], arr[ti * 3 + 1], arr[ti * 3 + 2]];
            }
            return [ti * 3, ti * 3 + 1, ti * 3 + 2];
        }

        const getPoints = (idx0, idx1, idx2, outA, outB, outC) => {
            outA.fromBufferAttribute(posAttr, idx0);
            outB.fromBufferAttribute(posAttr, idx1);
            outC.fromBufferAttribute(posAttr, idx2);
        }

        // --- Step 1: Precompute Face Normals and Areas ---
        const normals = new Float32Array(triCount * 3);
        const areas = new Float32Array(triCount);

        for (let i = 0; i < triCount; i++) {
            const [i0, i1, i2] = getTriIndices(i);
            getPoints(i0, i1, i2, v1, v2, v3);

            // Edge vectors
            const e1 = v2.sub(v1);
            const e2 = v3.clone().sub(v1);

            const n = new THREE.Vector3().crossVectors(e1, e2);
            const len = n.length();
            areas[i] = len * 0.5;

            if (len > 0) n.divideScalar(len);

            normals[i * 3] = n.x;
            normals[i * 3 + 1] = n.y;
            normals[i * 3 + 2] = n.z;
        }

        // --- Step 2: Build Adjacency Graph ---
        // Edge Map: "minIdx_maxIdx" -> [TriA, TriB]
        const adjacency = new Array(triCount).fill(null).map(() => []);

        if (isIndexed) {
            const edgeMap = new Map();
            for (let i = 0; i < triCount; i++) {
                const indices = getTriIndices(i);
                for (let j = 0; j < 3; j++) {
                    const a = indices[j];
                    const b = indices[(j + 1) % 3];
                    const key = a < b ? `${a}_${b}` : `${b}_${a}`;

                    let list = edgeMap.get(key);
                    if (!list) {
                        list = [];
                        edgeMap.set(key, list);
                    }
                    list.push(i);

                    if (list.length > 1) {
                        const neighbor = list[0];
                        adjacency[i].push(neighbor);
                        adjacency[neighbor].push(i);
                    }
                }
            }
        }

        // --- Step 3: Segment into Charts ---
        const visited = new Uint8Array(triCount); // 0: unvisited
        const charts = [];
        const thresholdDot = Math.cos(45 * Math.PI / 180);

        for (let i = 0; i < triCount; i++) {
            if (visited[i]) continue;

            const chart = {
                triangles: [],
                avgNormal: new THREE.Vector3()
            };

            const stack = [i];
            visited[i] = 1;

            // Flood Fill
            while (stack.length > 0) {
                const tIdx = stack.pop();
                chart.triangles.push(tIdx);

                const nA = new THREE.Vector3(normals[tIdx * 3], normals[tIdx * 3 + 1], normals[tIdx * 3 + 2]);
                chart.avgNormal.add(nA);

                const neighbors = adjacency[tIdx];
                for (let nIdx of neighbors) {
                    if (visited[nIdx]) continue;

                    const nB = new THREE.Vector3(normals[nIdx * 3], normals[nIdx * 3 + 1], normals[nIdx * 3 + 2]);
                    if (nA.dot(nB) > thresholdDot) {
                        visited[nIdx] = 1;
                        stack.push(nIdx);
                    }
                }
            }

            chart.avgNormal.normalize();
            charts.push(chart);
        }

        console.log(`Generated ${charts.length} UV charts.`);

        // --- Step 4: Project Charts ---
        const chartData = [];
        const totalPadding = 0.02; // Increased padding slightly

        for (let chart of charts) {
            // Calculate Basis
            const N = chart.avgNormal;
            let up = new THREE.Vector3(0, 1, 0);
            if (Math.abs(N.dot(up)) > 0.9) up.set(1, 0, 0);

            const tangent = new THREE.Vector3().crossVectors(up, N).normalize();
            const bitangent = new THREE.Vector3().crossVectors(N, tangent).normalize();

            const cData = {
                triangles: chart.triangles,
                localUVs: [],
                width: 0, height: 0,
                x: 0, y: 0
            };

            let uMin = Infinity, uMax = -Infinity;
            let vMin = Infinity, vMax = -Infinity;

            for (let tIdx of chart.triangles) {
                const [i0, i1, i2] = getTriIndices(tIdx);
                getPoints(i0, i1, i2, v1, v2, v3);

                const verts = [v1, v2, v3];
                for (let v of verts) {
                    const u = v.dot(tangent);
                    const v_ = v.dot(bitangent);
                    cData.localUVs.push(u, v_);

                    uMin = Math.min(uMin, u);
                    uMax = Math.max(uMax, u);
                    vMin = Math.min(vMin, v_);
                    vMax = Math.max(vMax, v_);
                }
            }

            // Normalize local UVs
            for (let k = 0; k < cData.localUVs.length; k += 2) {
                cData.localUVs[k] -= uMin;
                cData.localUVs[k + 1] -= vMin;
            }

            cData.width = (uMax - uMin) + totalPadding;
            cData.height = (vMax - vMin) + totalPadding;

            chartData.push(cData);
        }

        // --- Step 5: Pack Charts ---
        chartData.sort((a, b) => b.height - a.height);

        let currentX = 0;
        let currentY = 0;
        let rowHeight = 0;
        let maxMapSize = 0;

        let totalArea = 0;
        for (let c of chartData) totalArea += c.width * c.height;
        const targetWidth = Math.sqrt(totalArea) * 1.5; // More loose packing

        for (let c of chartData) {
            if (currentX + c.width > targetWidth) {
                currentX = 0;
                currentY += rowHeight;
                rowHeight = 0;
            }

            c.x = currentX;
            c.y = currentY;

            currentX += c.width;
            rowHeight = Math.max(rowHeight, c.height);

            maxMapSize = Math.max(maxMapSize, currentX);
        }
        const finalHeight = currentY + rowHeight;
        const finalScale = 1 / Math.max(Math.max(maxMapSize, targetWidth), finalHeight);

        // --- Step 6: Reconstruct Geometry ---
        const newVerts = new Float32Array(triCount * 3 * 3);
        const newNorms = new Float32Array(triCount * 3 * 3);
        const newUVs = new Float32Array(triCount * 3 * 2);

        let wV = 0;
        let wN = 0;
        let wU = 0;

        for (let c of chartData) {
            for (let k = 0; k < c.triangles.length; k++) {
                const tIdx = c.triangles[k];
                const [i0, i1, i2] = getTriIndices(tIdx);
                getPoints(i0, i1, i2, v1, v2, v3);

                newVerts[wV++] = v1.x; newVerts[wV++] = v1.y; newVerts[wV++] = v1.z;
                newVerts[wV++] = v2.x; newVerts[wV++] = v2.y; newVerts[wV++] = v2.z;
                newVerts[wV++] = v3.x; newVerts[wV++] = v3.y; newVerts[wV++] = v3.z;

                const nA = new THREE.Vector3(), nB = new THREE.Vector3(), nC = new THREE.Vector3();
                if (geometry.attributes.normal) {
                    nA.fromBufferAttribute(geometry.attributes.normal, i0);
                    nB.fromBufferAttribute(geometry.attributes.normal, i1);
                    nC.fromBufferAttribute(geometry.attributes.normal, i2);
                } else {
                    const fx = normals[tIdx * 3], fy = normals[tIdx * 3 + 1], fz = normals[tIdx * 3 + 2];
                    nA.set(fx, fy, fz); nB.set(fx, fy, fz); nC.set(fx, fy, fz);
                }
                newNorms[wN++] = nA.x; newNorms[wN++] = nA.y; newNorms[wN++] = nA.z;
                newNorms[wN++] = nB.x; newNorms[wN++] = nB.y; newNorms[wN++] = nB.z;
                newNorms[wN++] = nC.x; newNorms[wN++] = nC.y; newNorms[wN++] = nC.z;

                const luOffset = k * 6;
                const uv_u0 = (c.localUVs[luOffset] + c.x) * finalScale;
                const uv_v0 = (c.localUVs[luOffset + 1] + c.y) * finalScale;
                const uv_u1 = (c.localUVs[luOffset + 2] + c.x) * finalScale;
                const uv_v1 = (c.localUVs[luOffset + 3] + c.y) * finalScale;
                const uv_u2 = (c.localUVs[luOffset + 4] + c.x) * finalScale;
                const uv_v2 = (c.localUVs[luOffset + 5] + c.y) * finalScale;

                newUVs[wU++] = uv_u0; newUVs[wU++] = uv_v0;
                newUVs[wU++] = uv_u1; newUVs[wU++] = uv_v1;
                newUVs[wU++] = uv_u2; newUVs[wU++] = uv_v2;
            }
        }

        geometry.setIndex(null);
        geometry.setAttribute('position', new THREE.BufferAttribute(newVerts, 3));
        geometry.setAttribute('normal', new THREE.BufferAttribute(newNorms, 3));
        geometry.setAttribute('uv', new THREE.BufferAttribute(newUVs, 2));

        console.log("Smart UV Unwrap complete.");

    } catch (e) {
        console.error("UnwrapUVs Failed:", e);
        alert("Smart UV Unwrapping failed: " + e.message);
    }
}